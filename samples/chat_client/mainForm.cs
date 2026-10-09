namespace csharp_test_client;

public sealed class mainForm : Form
{
    private readonly TextBox host = new() { Text = "127.0.0.1", Width = 120 };
    private readonly NumericUpDown port = new() { Minimum = 1, Maximum = 65535, Value = 32453, Width = 80 };
    private readonly TextBox name = new() { Text = "Player", Width = 100 };
    private readonly NumericUpDown room = new() { Minimum = 0, Maximum = uint.MaxValue, Value = 1, Width = 80 };
    private readonly Button connect = new() { Text = "연결·입장", AutoSize = true };
    private readonly Button disconnect = new() { Text = "연결 종료", AutoSize = true, Enabled = false };
    private readonly TextBox message = new() { Width = 480 };
    private readonly Button send = new() { Text = "채팅 전송", AutoSize = true, Enabled = false };
    private readonly TextBox history = new() { Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, Dock = DockStyle.Fill };
    private readonly System.Windows.Forms.Timer heartbeat = new() { Interval = 5000 };
    private CancellationTokenSource? lifetime;
    private ChatConnection? connection;
    private bool connecting;

    public mainForm()
    {
        Text = "RIONetLite ChatClient — protocol 1";
        ClientSize = new Size(820, 460);
        MinimumSize = new Size(840, 480);
        var controls = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, Padding = new Padding(8) };
        controls.Controls.AddRange([new Label { Text = "주소", AutoSize = true }, host,
            new Label { Text = "포트", AutoSize = true }, port, new Label { Text = "이름", AutoSize = true }, name,
            new Label { Text = "방", AutoSize = true }, room, connect, disconnect]);
        var compose = new FlowLayoutPanel { Dock = DockStyle.Bottom, AutoSize = true, Padding = new Padding(8) };
        compose.Controls.AddRange([message, send]);
        Controls.Add(history);
        Controls.Add(compose);
        Controls.Add(controls);
        connect.Click += async (_, _) => await ConnectAsync();
        disconnect.Click += (_, _) => Disconnect();
        send.Click += async (_, _) => await SendChatAsync();
        message.KeyDown += async (_, args) =>
        {
            if (args.KeyCode == Keys.Enter) { args.SuppressKeyPress = true; await SendChatAsync(); }
        };
        heartbeat.Tick += async (_, _) => await SendHeartbeatAsync();
        FormClosed += (_, _) => { Disconnect(); heartbeat.Dispose(); };
        Append("이름은 UTF-8 1~32바이트, 채팅은 0~256바이트다. 같은 방의 모든 사용자에게 전달된다.");
    }

    private async Task ConnectAsync()
    {
        if (connecting || connection != null) return;
        connecting = true;
        connect.Enabled = false;
        var source = new CancellationTokenSource();
        lifetime = source;
        try
        {
            using var timeout = CancellationTokenSource.CreateLinkedTokenSource(source.Token);
            timeout.CancelAfter(TimeSpan.FromSeconds(5));
            var opened = await ChatConnection.OpenAsync(host.Text.Trim(), (int)port.Value, name.Text,
                (uint)room.Value, timeout.Token);
            if (source.IsCancellationRequested) { opened.Dispose(); return; }
            connection = opened;
            disconnect.Enabled = send.Enabled = true;
            heartbeat.Start();
            Append($"로그인·방 {(uint)room.Value} 입장 성공이다.");
            _ = ReadLoopAsync(opened, source.Token);
        }
        catch (Exception error) { Append($"연결 실패: {error.Message}"); if (!IsDisposed) Disconnect(); }
        finally { connecting = false; if (!IsDisposed) connect.Enabled = connection == null; }
    }

    private async Task ReadLoopAsync(ChatConnection opened, CancellationToken cancellation)
    {
        try
        {
            while (!cancellation.IsCancellationRequested)
            {
                ChatPacket packet = await opened.ReceiveAsync(cancellation);
                if (packet.Type != ChatType.ChatMessage) throw new InvalidDataException("unexpected response type");
                Append(ChatProtocol.DecodeChat(packet.Payload));
            }
        }
        catch (Exception error)
        {
            if (!cancellation.IsCancellationRequested) Append($"수신 종료: {error.Message}");
        }
        finally { if (ReferenceEquals(connection, opened)) Disconnect(); }
    }

    private async Task SendChatAsync()
    {
        var opened = connection;
        if (opened == null || lifetime == null) return;
        try
        {
            await opened.SendAsync(ChatProtocol.Chat(message.Text), lifetime.Token);
            if (!IsDisposed && ReferenceEquals(connection, opened)) message.Clear();
        }
        catch (ArgumentException error) { Append(error.Message); }
        catch (Exception error) { if (ReferenceEquals(connection, opened)) { Append($"송신 실패: {error.Message}"); Disconnect(); } }
    }

    private async Task SendHeartbeatAsync()
    {
        var opened = connection;
        if (opened == null || lifetime == null) return;
        try { await opened.SendAsync(ChatProtocol.Heartbeat(), lifetime.Token); }
        catch (Exception error) { if (ReferenceEquals(connection, opened)) { Append($"heartbeat 실패: {error.Message}"); Disconnect(); } }
    }

    private void Disconnect()
    {
        heartbeat.Stop();
        lifetime?.Cancel();
        lifetime?.Dispose();
        lifetime = null;
        connection?.Dispose();
        connection = null;
        disconnect.Enabled = send.Enabled = false;
        connect.Enabled = !connecting;
    }

    private void Append(string text)
    {
        if (IsDisposed) return;
        if (history.TextLength > 64000) history.Clear();
        history.AppendText($"{DateTime.Now:HH:mm:ss} {text}{Environment.NewLine}");
    }
}
