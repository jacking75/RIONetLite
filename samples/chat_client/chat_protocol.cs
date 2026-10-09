using System.Buffers.Binary;
using System.Text;

namespace csharp_test_client;

internal enum ChatType : ushort
{
    Login = 1, Join = 2, Chat = 3, Heartbeat = 4,
    LoginOk = 101, JoinOk = 102, ChatMessage = 103
}

internal readonly record struct ChatPacket(ChatType Type, byte[] Payload);

internal static class ChatProtocol
{
    public const int HeaderBytes = 8;
    public const int MaxPacketBytes = 1024;
    public const ushort Version = 1;
    public static readonly UTF8Encoding Utf8 = new(false, true);

    public static byte[] Encode(ChatType type, ReadOnlySpan<byte> payload)
    {
        if (payload.Length > MaxPacketBytes - HeaderBytes)
            throw new ArgumentOutOfRangeException(nameof(payload));
        byte[] packet = new byte[HeaderBytes + payload.Length];
        BinaryPrimitives.WriteUInt32BigEndian(packet, (uint)packet.Length);
        BinaryPrimitives.WriteUInt16BigEndian(packet.AsSpan(4), Version);
        BinaryPrimitives.WriteUInt16BigEndian(packet.AsSpan(6), (ushort)type);
        payload.CopyTo(packet.AsSpan(HeaderBytes));
        return packet;
    }

    private static byte[] StringPayload(string text, int maximum, bool allowEmpty)
    {
        byte[] bytes = Utf8.GetBytes(text);
        if (bytes.Length > maximum || (!allowEmpty && bytes.Length == 0))
            throw new ArgumentException($"UTF-8 길이가 {(allowEmpty ? 0 : 1)}~{maximum}바이트여야 한다.");
        byte[] payload = new byte[2 + bytes.Length];
        BinaryPrimitives.WriteUInt16BigEndian(payload, (ushort)bytes.Length);
        bytes.CopyTo(payload, 2);
        return payload;
    }

    public static byte[] Login(string name) => Encode(ChatType.Login, StringPayload(name, 32, false));
    public static byte[] Chat(string text) => Encode(ChatType.Chat, StringPayload(text, 256, true));
    public static byte[] Heartbeat() => Encode(ChatType.Heartbeat, []);
    public static byte[] Join(uint room)
    {
        byte[] payload = new byte[4];
        BinaryPrimitives.WriteUInt32BigEndian(payload, room);
        return Encode(ChatType.Join, payload);
    }

    public static (int Length, ChatType Type) DecodeHeader(ReadOnlySpan<byte> header)
    {
        if (header.Length != HeaderBytes) throw new InvalidDataException("header length");
        uint length = BinaryPrimitives.ReadUInt32BigEndian(header);
        if (length < HeaderBytes || length > MaxPacketBytes ||
            BinaryPrimitives.ReadUInt16BigEndian(header[4..]) != Version)
            throw new InvalidDataException("invalid packet length/version");
        return ((int)length, (ChatType)BinaryPrimitives.ReadUInt16BigEndian(header[6..]));
    }

    public static string DecodeChat(ReadOnlySpan<byte> payload)
    {
        if (payload.Length < 2) throw new InvalidDataException("chat length");
        ushort length = BinaryPrimitives.ReadUInt16BigEndian(payload);
        if (length > 256 || payload.Length != length + 2)
            throw new InvalidDataException("chat length");
        return Utf8.GetString(payload[2..]);
    }
}
