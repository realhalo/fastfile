/*
// CLIENT EXAMPLE CODE(using cygwin version ffcp.exe):
// use a remote fastfile key to transfer a local file to a remote destination.
// -------------------------------------------------------------------------------------------------------------------
try {
    // setup our fastfile client object to send to <destionation> using <key>.
    fastfile = new Fastfile(@"\PATH\TO\FFCP\DIR", "HOSTNAME_HERE", "KEY_HERE");

    // apply any custom/special options to the request. (complete list below)
//    fastfile.set("port", 8080);                     // alternate HTTP port.
//    fastfile.set("ssl", true);                      // use HTTPS. (changes default port to 443, unless specified)
//    fastfile.set("timeout", 20);                    // give up if idle activity timeout is exceeded. (seconds)
//    fastfile.set("bufSize", 16384);                 // (write) buffer size.
//    fastfile.set("connections", 15);                // number of async connections to send data on.
//    fastfile.set("failedConnections", 50);          // give up after X number of failed connections.
//    fastfile.set("ipv", 6);                         // explicit ipv4 or ipv6 protocol.
//    fastfile.set("location", "fastfile");           // alternate mod_fastfile location.
//    fastfile.set("chunkSize", 2097152);             // break file into X byte size chunks.
//    fastfile.set("host", "alt.domain.com");         // specify explicit "Host" header. (does not effect what is connected to)
//    fastfile.set("header", "HTTP-Header", "data");  // specify arbitrary HTTP headers. (multiple headers allowed)

    // transfer the file to the server.
    fastfile.send("PATH_TO_LOCAL_FILE_HERE");

    // ...asynchronously monitor fastfile.status (1-200 percentage) and fastfile.complete (enum of error, running, or success) from here...
}
catch (Exception ex) {
    MessageBox.Show("Fastfile fault: " + ex.Message);
}
// -------------------------------------------------------------------------------------------------------------------
*/

using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;

namespace Fastfile
{
    class Fastfile
    {
        private const string ffcpAllowedChars = @"[^a-zA-Z0-9=:. _/\\-]";
        private string ffcpPath = "";
        private int _status = 0; /* contains percentage, 0-200 (0-100 for manifesto building percentage, 100-200 for transfer percentage) */
        public int status { private set { _status = value; } get { return _status; } }
        private string _message = "";
        public string message { private set { _message = value; } get { return _message; } }
        public enum completeStatus { error, running, success }
        private completeStatus _complete = completeStatus.running;
        public completeStatus complete { private set { _complete = value; } get { return _complete; } }
        private BackgroundWorker statusWorker = null;
        private Process proc = null;
        private Dictionary<string, string> client = new Dictionary<string, string>();
        private ArrayList clientHeader = new ArrayList();

        public Fastfile(string path, string hostname, string key) {
            ffcpPath = path;
            client["hostname"] = hostname;
            client["key"] = key;

            /* process the status being streamed to stdout from ffcp.exe (Process.OutputDataReceived proved to be unreliable for this) */
            statusWorker = new BackgroundWorker();
            statusWorker.WorkerReportsProgress = true;
            statusWorker.WorkerSupportsCancellation = true;
            statusWorker.DoWork += new DoWorkEventHandler(statusWorker_DoWork);
            statusWorker.ProgressChanged += new ProgressChangedEventHandler(statusWorker_ProgressChanged);
        }
        ~Fastfile() { stop(); }

        public void stop() {
            try {
                if (proc != null && !proc.HasExited)
                    proc.Kill();
                if (statusWorker != null && statusWorker.IsBusy && !statusWorker.CancellationPending)
                    statusWorker.CancelAsync();
            }
            catch { }
        }

        /* set (optional) ffcp options. (return self to allow chaining of sets) */
        public Fastfile set<T>(string name, T value) {
            switch(name) {
                case "ssl":
                case "location":
                case "bufSize":
                case "timeout":
                case "chunkSize":
                case "connections":
                case "failedConnections":
                case "host":
                case "port":
                case "ipv":
                    client[name] = value.ToString();
                    break;
                case "header":
                    clientHeader.Add(value.ToString());
                    break;
                default:
                    throw new ArgumentException("Invalid set option", name);
            }
            return this;
        }

        /* set (optional) ffcp options. (this is currently only used for "header") (return self to allow chaining of sets) */
        public Fastfile set(string name, string key, string value) {
            set(name, key + "=" + value);
            return this;
        }

        /* send a file to a fastfile server. (using ffcp.exe) */
        public void send(string path) {
            if (proc != null)
                throw new Exception("Process is already active.");

            string arguments = "-q -a"; /* quiet errors and request api-mode output to stdout. */
            Dictionary<string, string> argumentMap = new Dictionary<string, string>();
            argumentMap["ssl"]                  = "-s";
            argumentMap["location"]             = "-l";
            argumentMap["bufSize"]              = "-b";
            argumentMap["timeout"]              = "-t";
            argumentMap["chunkSize"]            = "-c";
            argumentMap["connections"]          = "-n";
            argumentMap["failedConnections"]    = "-f";
            argumentMap["host"]                 = "-h";
            argumentMap["port"]                 = "-p";
            argumentMap["ipv"]                  = "-";
            argumentMap["header"]               = "-H";

            /* don't bother continuing if we can't find the file we're eventually going to send. */
            if (!File.Exists(path))
                throw new FileNotFoundException("Could not find file to send", path);

            /* create command-line argument string. */
            foreach (KeyValuePair<string, string> item in client) {
                if (argumentMap.ContainsKey(item.Key)) {
                    if (item.Key.Equals("ssl")) {
                        if (item.Value.Equals("True"))
                            arguments += " " + argumentMap[item.Key];
                    }
                    else if (item.Key.Equals("ipv")) {
                        if (item.Value.Equals("4") || item.Value.Equals("6"))
                            arguments += " " + argumentMap[item.Key] + item.Value;
                    }
                    else
                        arguments += " " + argumentMap[item.Key] + " \"" + Regex.Replace(item.Value, ffcpAllowedChars, "") + "\"";
                }
            }

            /* add in any specified HTTP header(s). */
            foreach (string header in clientHeader)
                arguments += " " + Regex.Replace(argumentMap["header"], ffcpAllowedChars, "") + " \"" + Regex.Replace(header, ffcpAllowedChars, "") + "\"";

            /* finally, add the hostname, key, and file to the argument list. */
            arguments += " \"" + Regex.Replace(this.client["hostname"], ffcpAllowedChars, "") + "\"";
            arguments += " \"" + Regex.Replace(this.client["key"], ffcpAllowedChars, "") + "\"";
            arguments += " \"" + path.Replace("\"", "") + "\""; /* quotes aren't allowed with windows filenames, just trim it and let it fail. */

            /* setup the process to be in the background and launch. */
            proc = new Process();
            proc.StartInfo.FileName = string.Format(@"{0}\{1}", ffcpPath, "ffcp.exe");
 
            proc.StartInfo.Arguments = arguments;
            proc.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;
            proc.StartInfo.UseShellExecute = false;
            proc.StartInfo.CreateNoWindow = true;
            proc.StartInfo.WorkingDirectory = ffcpPath;
            proc.StartInfo.RedirectStandardOutput = true;
            proc.StartInfo.RedirectStandardError = true;
            proc.StartInfo.StandardOutputEncoding = Encoding.GetEncoding("iso-8859-1"); /* we need to read the 8th bit of a character. */
            proc.Start();
            statusWorker.RunWorkerAsync();
        }

        /* monitor status returns from ffcp.exe. */
        private void statusWorker_DoWork(object sender, DoWorkEventArgs e) {
            int c;
            while (!statusWorker.CancellationPending && !proc.HasExited) {
                try {
                    if ((c = proc.StandardOutput.Read()) >= 0 && status != c)
                        statusWorker.ReportProgress(c);
                }
                catch { }
            }

            /* may have been tripped out of the loop before exiting, kill it if it's still going. */
            if (!proc.HasExited)
                proc.Kill();

            /* really just for the kill above, but it doesn't hurt to have this hit either way. */
            proc.WaitForExit();

            /* record the success/error status. */
            if (statusWorker.CancellationPending)
                message = "User aborted transfer.";
            else if (proc.ExitCode != 0) {
                try { message = proc.StandardError.ReadToEnd(); }
                catch { message = "Unknown error."; }
            }
            else
                message = "Successfully sent file!";

            complete = proc.ExitCode == 0 ? completeStatus.success : completeStatus.error;
        }

        private void statusWorker_ProgressChanged(object sender, ProgressChangedEventArgs e) {
            status = e.ProgressPercentage;
        }
   }
}
