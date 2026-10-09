using System.Buffers.Binary;
using System.Net.Sockets;

namespace csharp_test_client;

// One reader owns ReceiveAsync. A semaphore serializes complete packet sends,
// so heartbeat and UI sends cannot interleave bytes on the TCP stream.
internal sealed class ChatConnection : IDisposable
{
    private readonly Socket socket = new(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
    private readonly SemaphoreSlim sendGate = new(1, 1);
    private int disposed;

    public bool HasPendingBytes => socket.Available != 0;

    public static async Task<ChatConnection> OpenAsync(string host, int port, string name, uint room,
        CancellationToken cancellation)
    {
        var connection = new ChatConnection();
        try
        {
            byte[] login = ChatProtocol.Login(name);
            await connection.socket.ConnectAsync(host, port, cancellation).ConfigureAwait(false);
            connection.socket.NoDelay = true;
            await connection.SendAsync(login, cancellation).ConfigureAwait(false);
            ChatPacket response = await connection.ReceiveAsync(cancellation).ConfigureAwait(false);
            if (response.Type != ChatType.LoginOk || response.Payload.Length != 0)
                throw new InvalidDataException("login response");
            await connection.SendAsync(ChatProtocol.Join(room), cancellation).ConfigureAwait(false);
            response = await connection.ReceiveAsync(cancellation).ConfigureAwait(false);
            if (response.Type != ChatType.JoinOk || response.Payload.Length != 4 ||
                BinaryPrimitives.ReadUInt32BigEndian(response.Payload) != room)
                throw new InvalidDataException("join response");
            return connection;
        }
        catch { connection.Dispose(); throw; }
    }

    public async Task SendAsync(ReadOnlyMemory<byte> bytes, CancellationToken cancellation)
    {
        await sendGate.WaitAsync(cancellation).ConfigureAwait(false);
        try
        {
            while (!bytes.IsEmpty)
            {
                int count = await socket.SendAsync(bytes, SocketFlags.None, cancellation).ConfigureAwait(false);
                if (count == 0) throw new EndOfStreamException("send closed");
                bytes = bytes[count..];
            }
        }
        finally { sendGate.Release(); }
    }

    public async Task<ChatPacket> ReceiveAsync(CancellationToken cancellation)
    {
        byte[] header = new byte[ChatProtocol.HeaderBytes];
        await ReadExactlyAsync(header, cancellation).ConfigureAwait(false);
        var (length, type) = ChatProtocol.DecodeHeader(header);
        byte[] payload = new byte[length - ChatProtocol.HeaderBytes];
        await ReadExactlyAsync(payload, cancellation).ConfigureAwait(false);
        return new ChatPacket(type, payload);
    }

    private async Task ReadExactlyAsync(Memory<byte> destination, CancellationToken cancellation)
    {
        while (!destination.IsEmpty)
        {
            int count = await socket.ReceiveAsync(destination, SocketFlags.None, cancellation).ConfigureAwait(false);
            if (count == 0) throw new EndOfStreamException("서버가 연결을 종료했다.");
            destination = destination[count..];
        }
    }

    public void Dispose()
    {
        if (Interlocked.Exchange(ref disposed, 1) != 0) return;
        socket.Dispose();
        // In-flight send continuations can still release the managed semaphore.
    }
}
