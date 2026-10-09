using System.Net.Sockets;
using System.Text.Json;

namespace csharp_test_client;

// Headless checks use the same encoder and transport as the WinForms UI.
internal static class ChatChecks
{
    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidDataException(message);
    }

    private static void VerifyFixtures(string path)
    {
        int count = 0;
        foreach (string line in File.ReadLines(path))
        {
            if (line.Length == 0 || line.StartsWith('#')) continue;
            string[] fields = line.Split('\t');
            Require(fields.Length == 4, "fixture columns");
            var type = (ChatType)ushort.Parse(fields[1]);
            byte[] payload = fields[2] == "-" ? [] : Convert.FromHexString(fields[2]);
            byte[] encoded = fields[0] switch
            {
                "login_a" => ChatProtocol.Login("A"),
                "join_1" => ChatProtocol.Join(1),
                "chat_hi" => ChatProtocol.Chat("hi"),
                "chat_korean" => ChatProtocol.Chat("안녕"),
                "heartbeat" => ChatProtocol.Heartbeat(),
                _ => ChatProtocol.Encode(type, payload)
            };
            byte[] expected = Convert.FromHexString(fields[3]);
            Require(encoded.AsSpan().SequenceEqual(expected), $"fixture {fields[0]}");
            var header = ChatProtocol.DecodeHeader(expected.AsSpan(0, ChatProtocol.HeaderBytes));
            Require(header.Length == expected.Length && header.Type == type, $"decode {fields[0]}");
            ++count;
        }
        Require(count == 8, "fixture count");
        ExpectArgumentFailure(() => ChatProtocol.Login(""));
        ExpectArgumentFailure(() => ChatProtocol.Login(new string('a', 33)));
        ExpectArgumentFailure(() => ChatProtocol.Chat(new string('a', 257)));
        Require(ChatProtocol.Login(new string('a', 32)).Length == 42, "maximum login");
        Require(ChatProtocol.Chat(new string('a', 256)).Length == 266, "maximum chat");
    }

    private static void ExpectArgumentFailure(Func<byte[]> action)
    {
        try { action(); }
        catch (ArgumentException) { return; }
        throw new InvalidDataException("expected local validation failure");
    }

    private static async Task ExpectChatAsync(ChatConnection peer, string expected)
    {
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(3));
        ChatPacket packet = await peer.ReceiveAsync(timeout.Token);
        Require(packet.Type == ChatType.ChatMessage && ChatProtocol.DecodeChat(packet.Payload) == expected,
            "chat recipient/content mismatch");
    }

    private static async Task ExpectQuietAsync(ChatConnection peer)
    {
        using var timeout = new CancellationTokenSource(TimeSpan.FromMilliseconds(150));
        try
        {
            ChatPacket unexpected = await peer.ReceiveAsync(timeout.Token);
            throw new InvalidDataException($"unexpected packet {(ushort)unexpected.Type}");
        }
        catch (OperationCanceledException) when (timeout.IsCancellationRequested) { }
        Require(!peer.HasPendingBytes, "partial unexpected packet");
    }

    private static async Task ExpectClosedAsync(ChatConnection peer)
    {
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(3));
        try { await peer.ReceiveAsync(timeout.Token); }
        catch (EndOfStreamException) { return; }
        catch (SocketException error) when (error.SocketErrorCode == SocketError.ConnectionReset ||
            error.SocketErrorCode == SocketError.ConnectionAborted) { return; }
        throw new InvalidDataException("invalid request did not close connection");
    }

    public static async Task<int> RunAsync(string[] args)
    {
        bool selfTest = false;
        int port = 32453;
        string? fixtures = null, jsonPath = null;
        for (int index = 0; index < args.Length; ++index)
        {
            switch (args[index])
            {
                case "--self-test": selfTest = true; break;
                case "--port" when index + 1 < args.Length: port = int.Parse(args[++index]); break;
                case "--fixtures" when index + 1 < args.Length: fixtures = args[++index]; break;
                case "--json" when index + 1 < args.Length: jsonPath = args[++index]; break;
                default: throw new ArgumentException("usage: ChatClient [--self-test] --fixtures PATH [--port N] [--json PATH]");
            }
        }
        if (fixtures == null) throw new ArgumentException("--fixtures is required for checks");
        VerifyFixtures(fixtures);
        int chats = 0, recipients = 0, malformed = 0;
        if (selfTest)
        {
            using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(30));
            CancellationToken cancellation = timeout.Token;
            using var first = await ChatConnection.OpenAsync("127.0.0.1", port, "A", 1, cancellation);
            using var second = await ChatConnection.OpenAsync("127.0.0.1", port, "B", 1, cancellation);
            using var third = await ChatConnection.OpenAsync("127.0.0.1", port, new string('C', 32), 1, cancellation);
            using var otherRoom = await ChatConnection.OpenAsync("127.0.0.1", port, "D", 2, cancellation);
            ChatConnection[] room = [first, second, third];

            // Exercise TCP raw spans much larger than one event/packet.
            foreach (int size in new[] { 2048, 4096, 16384 })
            {
                byte[] coalesced = new byte[size];
                byte[] heartbeat = ChatProtocol.Heartbeat();
                for (int offset = 0; offset < size; offset += heartbeat.Length)
                    heartbeat.CopyTo(coalesced, offset);
                await first.SendAsync(coalesced, cancellation);
                string text = $"coalesced-{size}";
                await first.SendAsync(ChatProtocol.Chat(text), cancellation);
                foreach (ChatConnection peer in room) { await ExpectChatAsync(peer, text); ++recipients; }
                ++chats;
            }

            byte[] split = ChatProtocol.Chat("한글 분할 전송");
            for (int offset = 0; offset < split.Length; ++offset)
                await second.SendAsync(split.AsMemory(offset, 1), cancellation);
            foreach (ChatConnection peer in room) { await ExpectChatAsync(peer, "한글 분할 전송"); ++recipients; }
            ++chats;

            string maximum = new('x', 256);
            await third.SendAsync(ChatProtocol.Chat(maximum), cancellation);
            foreach (ChatConnection peer in room) { await ExpectChatAsync(peer, maximum); ++recipients; }
            ++chats;

            // Empty chat is valid, heartbeat has no response, and another room
            // receives neither the five broadcasts above nor their duplicates.
            await first.SendAsync(ChatProtocol.Chat(""), cancellation);
            foreach (ChatConnection peer in room) { await ExpectChatAsync(peer, ""); ++recipients; }
            ++chats;
            await ExpectQuietAsync(otherRoom);
            await otherRoom.SendAsync(ChatProtocol.Chat("room-2-only"), cancellation);
            await ExpectChatAsync(otherRoom, "room-2-only");
            foreach (ChatConnection peer in room) await ExpectQuietAsync(peer);

            second.Dispose();
            using var rejoined = await ChatConnection.OpenAsync("127.0.0.1", port, "B2", 1, cancellation);
            await rejoined.SendAsync(ChatProtocol.Chat("reconnected"), cancellation);
            foreach (ChatConnection peer in new[] { first, third, rejoined })
            { await ExpectChatAsync(peer, "reconnected"); ++recipients; }
            ++chats;

            // Reject invalid UTF-8, unknown type, invalid/oversized length and version.
            byte[][] invalid =
            [
                ChatProtocol.Encode(ChatType.Chat, [0, 1, 0xff]),
                ChatProtocol.Encode((ChatType)99, []),
                [0, 0, 0, 7, 0, 1, 0, 4],
                [0, 0, 4, 1, 0, 1, 0, 4],
                [0, 0, 0, 8, 0, 2, 0, 4]
            ];
            foreach (byte[] packet in invalid)
            {
                using var bad = await ChatConnection.OpenAsync("127.0.0.1", port, "invalid", 9, cancellation);
                await bad.SendAsync(packet, cancellation);
                await ExpectClosedAsync(bad);
                ++malformed;
            }
        }
        string json = JsonSerializer.Serialize(new
        {
            passed = true, protocol_version = ChatProtocol.Version, fixtures = 8,
            self_test = selfTest, port, chat_requests = chats,
            broadcast_receipts = recipients, malformed_closes = malformed
        }, new JsonSerializerOptions { WriteIndented = true });
        Console.WriteLine(json);
        if (jsonPath != null) await File.WriteAllTextAsync(jsonPath, json);
        return 0;
    }
}
