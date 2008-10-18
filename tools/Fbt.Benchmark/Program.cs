using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using Fbt.Service;

namespace Fbt.Benchmark
{
    static class Helpers
    {
        public static void RunShellCommand(string cmd)
        {
            System.Diagnostics.Process proc =
                System.Diagnostics.Process.Start("cmd.exe", "/C \"" + cmd + "\"");
            proc.WaitForExit();

            if (proc.ExitCode != 0)
            {
                throw new ApplicationException(
                    String.Format(
                        "Command '{0}' failed with exit code {1}", cmd, proc.ExitCode));
            }
        }
    }

    class Testrun
    {
        private uint bufferCount;
        private uint bufferSize;
        private string traceFile;
        private string[] routineMasks;

        public delegate void StressSystemDelegate();

        public Testrun(
            uint bufferCount,
            uint bufferSize,
            string traceFile,
            string[] routineMasks)
        {
            this.bufferCount = bufferCount;
            this.bufferSize = bufferSize;
            this.traceFile = traceFile;
            this.routineMasks = routineMasks;
        }

        public TimeSpan run(StressSystemDelegate stress)
        {
            using (Native.CommandProcessor proc = new Native.CommandProcessor(
                Console.Write,
                Native.JPFSV_KERNEL))
            {
                if (this.routineMasks != null)
                {
                    Console.WriteLine("Attaching...");
                    proc.ProcessCommand(
                        String.Format(
                            ".attach 0n{0} 0n{1} {2}",
                            this.bufferCount,
                            this.bufferSize,
                            this.traceFile));
                }

                Console.WriteLine("Starting perfmon...");
                Helpers.RunShellCommand("logman start KFBT");

                if (this.routineMasks != null)
                {
                    foreach (string mask in this.routineMasks)
                    {
                        Console.WriteLine("Adding instrumentation for {0}...", mask);
                        proc.ProcessCommand(
                            String.Format("tp {0}", mask));
                    }
                }

                Stopwatch watch = new Stopwatch();
                watch.Start();

                stress();

                watch.Stop();

                if (this.routineMasks != null)
                {
                    foreach (string mask in this.routineMasks)
                    {
                        Console.WriteLine("Revoking instrumentation for {0}...", mask);
                        proc.ProcessCommand(String.Format("tc {0}", mask));
                    }
                }

                Console.WriteLine("Stopping perfmon...");
                Helpers.RunShellCommand("logman stop KFBT");

                return watch.Elapsed;
            }
        }
    }

    class Program
    {
        private const uint BUFFER_COUNT = 0x200;
        //private const uint BUFFER_SIZE = 0x10000;
        private const uint BUFFER_SIZE = 0x3ff00;
        private const String TRACE_BASE_DIR = @"f:\";
        private const String WRK_BASE_DIR = @"c:\home\wrk-v1.2\";

        static void Main(string[] args)
        {
            if (args.Length < 1)
            {
                Console.WriteLine("Usage: benchmrk <name> [<mask>*]");
                return;
            }


            string name = args[0];
            string[] masks;
            if (args.Length < 2)
            {
                masks = null;
            }
            else
            {
                masks = new string[args.Length - 1];
                Array.Copy(args, 1, masks, 0, masks.Length);
            }
            
            if (masks == null)
            {
                Console.WriteLine("No mask provided - creating baseline");
            }

            Testrun run = new Testrun(
                BUFFER_COUNT,
                BUFFER_SIZE,
                TRACE_BASE_DIR + name + ".jtrc",
                masks);
            TimeSpan elapsed = run.run(
                delegate {
                    for (int i = 0; i < 1; i++)
                    {
                        Console.Write("Build #{0}...", i);
                        Stopwatch watch = new Stopwatch();
                        watch.Start();

                        Helpers.RunShellCommand(
                            String.Format("cd /d \"{0}\" && setenv.bat && clean.bat && build.bat", WRK_BASE_DIR));

                        watch.Stop();
                        Console.WriteLine("{0} ms", watch.ElapsedMilliseconds);
                    }
                });

            Console.WriteLine("{0} elapsed ({1} ms)", elapsed, elapsed.TotalMilliseconds);

            //
            // Forcefully exit.
            //
            Console.WriteLine("Committing suicide...");
            System.Diagnostics.Process.GetCurrentProcess().Kill();
        }
    }
}
