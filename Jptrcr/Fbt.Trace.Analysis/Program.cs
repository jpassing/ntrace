using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using Fbt.Trace.Reader;
using Wintellect.PowerCollections;

namespace Fbt.Trace.Analysis
{
    class Program
    {
        private delegate string GetNameDelegate(TraceCall call);

        private class RankingItem : IComparable<RankingItem>
        {
            public int CallCount;
            public string FunctionName;

            public RankingItem(string funcName, int callCount)
            {
                this.CallCount = callCount;
                this.FunctionName = funcName;
            }

            public int CompareTo(RankingItem other)
            {
                return this.CallCount - other.CallCount;
            }
        }

        private static void Traverse(
            ICollection<TraceCall> calls,
            Dictionary<string, RankingItem> routines,
            ref uint totalCount,
            GetNameDelegate getName)
        {
            foreach (TraceCall call in calls)
            {
                if (routines.ContainsKey(getName(call)))
                {
                    RankingItem item = routines[getName(call)];
                    item.CallCount++;
                }
                else
                {
                    routines[getName(call)] =
                        new RankingItem(getName(call), 1);
                }

                totalCount++;

                if (!call.IsSynthetic)
                {
                    Traverse(call.Calls, routines, ref totalCount, getName);
                }
            }
        }

        private static void ShowMostCalled(
            TraceFile file,
            int count,
            GetNameDelegate getName)
        {
            Dictionary<string, RankingItem> routines = 
                new Dictionary<string, RankingItem>();
            uint totalCount = 0;

            //
            // Build histogram.
            //
            foreach (TraceClient client in file.Clients)
            {
                Traverse(client.Calls, routines, ref totalCount, getName);
            }

            //
            // Create ranking.
            //
            OrderedBag<RankingItem> ranking = new OrderedBag<RankingItem>();
            foreach (RankingItem item in routines.Values)
            {
                ranking.Add(item);
            }

            int index = 0;
            foreach (RankingItem item in ranking.Reversed())
            {
                if (index++ == count)
                {
                    break;
                }

                Console.WriteLine("{0,60}\t{1}", item.FunctionName, item.CallCount);
            }

            Console.WriteLine();
            Console.WriteLine("{0} events total", totalCount);
        }

        private static string GetFunctionNamePrefix(string funcName)
        {
            int caseSeitches = 0;
            bool upcase = true;
            for (int i = 0; i < funcName.Length; i++)
            {
                bool isUpper = Char.IsUpper(funcName[i]);
                if (upcase != isUpper)
                {
                    if ( ++caseSeitches == 2 )
                    {
                        return funcName.Substring(0,i);
                    }
                    else
                    {
                        upcase = isUpper;
                    }
                }
            }

            return funcName;
        }

        public static void Main(string[] args)
        {
            if (args.Length < 2)
            {
                Console.WriteLine("Usage:");
                Console.WriteLine("  trcan <command> <file>");
                Console.WriteLine();
                Console.WriteLine("Commands are:");
                Console.WriteLine("  top");
                Console.WriteLine("  prefix");
                Environment.ExitCode = 1;
                return;
            }

            string command = args[0];
            string file = args[1];

            if (!File.Exists(file))
            {
                Console.WriteLine("File not found");
                Environment.ExitCode = 1;
                return;
            }

            try
            {
                Console.Write("Loading file...");
                using (TraceFile traceFile = new TraceFile(file))
                {
                    Console.WriteLine("done.");

                    switch (command)
                    {
                        case "top":
                            ShowMostCalled(
                                traceFile, 
                                100,
                                delegate (TraceCall call) {
                                    return call.ToString();
                                });
                            break;

                        case "prefix":
                            ShowMostCalled(
                                traceFile,
                                100,
                                delegate(TraceCall call)
                                {
                                    return GetFunctionNamePrefix(call.FunctionName);
                                });
                            break;

                        default:
                            Console.WriteLine("Unrecognized command");
                            Environment.ExitCode = 1;
                            return;
                    }
                }
            }
            catch (Exception x)
            {
                Console.WriteLine(x.Message);
                Environment.ExitCode = 1;
                return;
            }
        }
    }
}
