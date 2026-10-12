// A session through WinSCP's .NET assembly, as a user's code drives it: the
// assembly starts the executable with /console /consoleinstance (Windows) or
// /consolepipes (elsewhere) and speaks winscp.com's protocol to it. Arguments:
//   executable protocol(sftp|ftp|ftpes|webdav|s3) port remote-dir local-dir
// The servers are run_dotnet.py's: user linscp, password secret on
// 127.0.0.1 (S3: key LINSCPKEY, secret "secret", path-style, plain HTTP).
using System;
using System.IO;
using System.Linq;
using WinSCP;

internal static class Program
{
    private static int Failures;

    private static void Check(bool condition, string what)
    {
        if (!condition)
        {
            Console.WriteLine("FAIL " + what);
            Failures++;
        }
    }

    private static int Main(string[] args)
    {
        string executable = args[0], protocol = args[1], remote = args[3], local = args[4];
        int port = int.Parse(args[2]);
        var options = new SessionOptions
        {
            HostName = "127.0.0.1",
            PortNumber = port,
            UserName = protocol == "s3" ? "LINSCPKEY" : "linscp",
            Password = "secret",
        };
        switch (protocol)
        {
            case "sftp":
                options.Protocol = Protocol.Sftp;
                options.GiveUpSecurityAndAcceptAnySshHostKey = true;
                break;
            case "ftp":
                options.Protocol = Protocol.Ftp;
                break;
            case "ftpes":
                options.Protocol = Protocol.Ftp;
                options.FtpSecure = FtpSecure.Explicit;
                options.GiveUpSecurityAndAcceptAnyTlsHostCertificate = true;
                break;
            case "webdav":
                options.Protocol = Protocol.Webdav;
                break;
            case "s3":
                options.Protocol = Protocol.S3;
                options.Secure = false;
                options.AddRawSettings("S3UrlStyle", "1");
                break;
        }

        try
        {
            using (var session = new Session())
            {
                session.ExecutablePath = executable;
                // The assembly checks the executable's version resource, which
                // only a Windows executable has.
                session.DisableVersionCheck = !OperatingSystem.IsWindows();
                int transferred = 0;
                session.FileTransferred += (sender, e) => transferred++;
                session.Open(options);
                Check(session.Opened, "the session opens");

                RemoteDirectoryInfo listing = session.ListDirectory(remote);
                Check(listing.Files.Any(f => f.Name == "hello.txt" && f.Length == 14 && !f.IsDirectory),
                      "ListDirectory finds hello.txt, 14 bytes");
                Check(listing.Files.Any(f => f.Name == "sub" && f.IsDirectory), "ListDirectory finds the sub directory");

                TransferOperationResult download = session.GetFiles(remote + "/hello.txt", local + Path.DirectorySeparatorChar);
                download.Check();
                Check(File.ReadAllText(Path.Combine(local, "hello.txt")) == "Hello, linscp\n", "GetFiles downloads");

                File.WriteAllText(Path.Combine(local, "upload.txt"), "uploaded through .NET\n");
                session.PutFiles(Path.Combine(local, "upload.txt"), remote + "/").Check();
                Check(session.FileExists(remote + "/upload.txt"), "PutFiles uploads; FileExists sees it");

                session.CreateDirectory(remote + "/newdir");
                session.MoveFile(remote + "/upload.txt", remote + "/newdir/moved.txt");
                Check(session.FileExists(remote + "/newdir/moved.txt") && !session.FileExists(remote + "/upload.txt"),
                      "CreateDirectory and MoveFile");
                RemoteFileInfo info = session.GetFileInfo(remote + "/newdir/moved.txt");
                Check(info.Length == "uploaded through .NET\n".Length, "GetFileInfo reports the size");

                session.RemoveFiles(remote + "/newdir/moved.txt").Check();
                session.RemoveFiles(remote + "/newdir").Check();
                Check(!session.FileExists(remote + "/newdir"), "RemoveFiles removes a file and a directory");

                // A failure is reported, not thrown, by the operation result.
                TransferOperationResult missing = session.GetFiles(remote + "/missing.txt", local + Path.DirectorySeparatorChar);
                Check(!missing.IsSuccess && missing.Failures.Count > 0, "a failed GetFiles reports its failure");

                Check(transferred >= 2, $"FileTransferred fires ({transferred})");
            }
        }
        catch (Exception e)
        {
            Console.WriteLine("FAIL exception: " + e);
            Failures++;
        }
        if (Failures == 0)
        {
            Console.WriteLine("OK");
        }
        return Failures == 0 ? 0 : 1;
    }
}
