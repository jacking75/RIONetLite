namespace csharp_test_client;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        if (args.Length != 0)
        {
            try { return ChatChecks.RunAsync(args).GetAwaiter().GetResult(); }
            catch (Exception error)
            {
                Console.Error.WriteLine(error);
                return 1;
            }
        }
        ApplicationConfiguration.Initialize();
        Application.Run(new mainForm());
        return 0;
    }
}
