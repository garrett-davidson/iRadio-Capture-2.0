//Copyright 2013 Garrett Davidson
//
//This file is part of iRadio Capture 2.0.
//
//    iRadio Capture 2.0 is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    iRadio Capture 2.0 is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with iRadio Capture 2.0.  If not, see <http://www.gnu.org/licenses/>.


using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;
using System.Threading;
using TagLib;
using iTunesLib;
using iRadio_Capture_2._0.Properties;

namespace iRadio_Capture_2._0
{
    public partial class Form1 : Form
    {
        FileSystemWatcher downloadsWatcher;
        FileSystemWatcher songWatcher;
        bool songFound;
        string songPath;
        string songFolder;
        string previousImagePath;
        bool started;
        bool taggingInProgress = false;
        bool songBuffering = false;
        System.Diagnostics.Process assnifferProcess;

        public Form1()
        {
            InitializeComponent();
            downloadsWatcher = new FileSystemWatcher(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile) + @"\Downloads");
            downloadsWatcher.Renamed += downloadsWatcher_Renamed;
            string path = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + @"\iRadio Capture 2.0";
            if (!Directory.Exists(path)) Directory.CreateDirectory(path);
            songWatcher = new FileSystemWatcher(path);
            Console.WriteLine(path);
            songWatcher.Created += songWatcher_Created;

            songFound = false;
            statusLabel.Text = "";

            this.FormClosed += Form1_FormClosed;
            

            if (Settings.Default.songStorage == "")
            {
                string paths = Environment.GetFolderPath(Environment.SpecialFolder.MyMusic);
                paths += @"\iRadio Capture";
                Settings.Default.songStorage = paths;
                Settings.Default.Save();
            }

            //clean up from the previous time
            //in case of unexpected closing
            Form1_FormClosed(null, null);
        }

        void Form1_FormClosed(object sender, FormClosedEventArgs e)
        {

            //clean up
            try
            {
                if (assnifferProcess != null) assnifferProcess.Kill();
            }

            catch { }

            try
            {
                string path = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + @"\iRadio Capture 2.0";
                string[] dirs = Directory.GetDirectories(path);
                foreach (string dir in dirs)
                {
                    Directory.Delete(dir, true);
                }
                string[] files = Directory.GetFiles(path);
                //pictureBox1.Image.Dispose();
                foreach (string file in files)
                {
                    System.IO.File.Delete(file);
                }
            }

            catch { };
        }

        void songWatcher_Created(object sender, FileSystemEventArgs e)
        {
            //do NOT remove this
            //sometimes this event fires before all subfolders are created
            //causing crashes/unpredictable behavior
            Thread.Sleep(500);

            while (taggingInProgress)
            {
                Thread.Sleep(5000);
            }

            //if its not album art
            if (!e.Name.Contains(@".jpg"))
            {
                Console.WriteLine(e.FullPath);
                string sub = e.FullPath;
                string[] stuff;
                string[] contents;
                string song = null;
                while (song == null)
                {
                    stuff = Directory.GetDirectories(sub);
                    

                    if (stuff.Count() > 0)
                    {
                        sub = stuff[0];
                    }

                    else
                    {
                        contents = Directory.GetFiles(sub);
                        song = contents[0];
                    }
                }
                FileInfo inf = new FileInfo(song);
                long size = 0;
                songBuffering = true;
                while (size != inf.Length)
                {
                    size = inf.Length;
                    Thread.Sleep(5000);
                    inf.Refresh();
                }
                songBuffering = false;

                songPath = song;
                songFolder = e.FullPath;
                //this must come LAST
                //otherwise downloadsWatcher_Renamed may continue before the above finishes
                songFound = true;
            }
        }

        void downloadsWatcher_Renamed(object sender, FileSystemEventArgs e)
        {

            while (taggingInProgress)
            {
                Thread.Sleep(5000);
            }

            if (e.Name.Contains(@".jpg"))
            {
                //while (e.Name.Contains(@".crdownload"))
                {
                //    Thread.Sleep(500);
                }

                if (!e.Name.Contains(@".crdownload"))
                {
                    
                    string newImagePath = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + @"\iRadio Capture 2.0\" + e.Name;

                    //Not functionally significant
                    //However removing causes Chrome to say:
                    //"Anti-virus software failed unexpectedly while scanning this file."
                    //when each picture is downloaded
                    Thread.Sleep(100);


                    System.IO.File.Move(e.FullPath, newImagePath);                    
                    
                    #region Update UI

                    string[] tags = e.Name.Split('_');

                    updateUI(tags, newImagePath);

                    #endregion

                    while (!songFound)
                    {
                        Thread.Sleep(5000);
                    }
                    taggingInProgress = true;
                    songFound = false;

                    #region Song Tagging
                    int oldBitrate;
                    

                    //must change the extension for Last.fm
                    if (tags[3].Contains(@"last"))
                    {
                        string newName = songPath.Substring(0, songPath.Length - 5) + @".mp3";
                        bool renamed = false;
                        while (!renamed)
                        {
                            try
                            {
                                System.IO.File.Move(songPath, newName);
                                renamed = true;
                            }

                            catch
                            { }
                        }
                        songPath = newName;
                    }
                    TagLib.File newSong = TagLib.File.Create(@songPath);

                    newSong.Tag.Title = tags[0];
                    string[] artists = new string[1];
                    artists[0] = tags[1];
                    newSong.Tag.Performers = artists;
                    newSong.Tag.Album = tags[2];
                    TagLib.Picture pic = new TagLib.Picture(newImagePath);
                    IPicture[] pics = new IPicture[1];
                    pics[0] = pic;
                    newSong.Tag.Pictures = pics;
                    newSong.Save();
                    oldBitrate = newSong.Properties.AudioBitrate;

                    #endregion

                    
                    bool duplicate = false;
                    bool overWritten = false;

                    //the if statement is not in the region
                    //so you can step over it without expanding the region
                    if (Settings.Default.customFolder)
                    #region Custom Folder Duplicate Checking and Song Saving
                    //Add song to custom folder
                    
                    {

                        string path = Settings.Default.songStorage;

                        if (!Directory.Exists(path)) Directory.CreateDirectory(path);

                        path += "\\" + tags[1];
                        if (!Directory.Exists(path)) Directory.CreateDirectory(path);

                        path += "\\" + tags[2];
                        if (!Directory.Exists(path)) Directory.CreateDirectory(path);

                        path += "\\" + tags[0] + @".m4a";
                        if (!System.IO.File.Exists(path)) System.IO.File.Move(songPath, path);

                        else
                        {
                            duplicate = true;
                            //check if current song has higher bitrate
                            //duplicateSongPath = path;
                            TagLib.File oldsong = TagLib.File.Create(path);

                            if (Settings.Default.overwriteLowerBitrate)
                            {
                                if (oldBitrate > oldsong.Properties.AudioBitrate)
                                {
                                    System.IO.File.Delete(path);
                                    System.IO.File.Move(songPath, path);
                                    overWritten = true;
                                }
                            }
                        }
                    }

#endregion


                    #region iTunes Duplicate Checking and Song Saving
                    //Add song to iTunes
                    else
                    {
                        IITTrack oldTrack = null;

                        iTunesApp iTunes = new iTunesApp();
                        IITLibraryPlaylist library = iTunes.LibraryPlaylist;
                        IITTrackCollection tracks = library.Tracks;

                        //basic duplicate detection
                        IITTrack currentSong = tracks.get_ItemByName(tags[0]);
                        if (currentSong != null)
                        {
                            if (currentSong.Artist == tags[1] && currentSong.Album == tags[2])
                            {
                                duplicate = true;
                                oldTrack = currentSong;
                            }


                            //advanced duplicate detection
                            //used in the case that there are different songs with the same name
                            else
                            {
                                foreach (IITTrack track in tracks)
                                {
                                    if (track.Name == tags[0])
                                    {
                                        if (track.Album == tags[2])
                                        {
                                            if (track.Artist == tags[1])
                                            {
                                                duplicate = true;
                                                oldTrack = track;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        if (!duplicate)
                        {
                            //add to Library
                            library.AddFile(songPath);

                            //add to playlist
                            if (Settings.Default.addToiTunesPlaylist)
                            {
                                string playlistName = Settings.Default.iTunesPlaylist;
                                IITUserPlaylist playlist = (IITUserPlaylist)library.Source.Playlists.get_ItemByName(playlistName);
                                if (playlist == null)
                                {
                                    iTunes.CreatePlaylist(playlistName);
                                    playlist = (IITUserPlaylist)library.Source.Playlists.get_ItemByName(playlistName);
                                }
                                playlist.AddFile(songPath);
                            }
                        }

                        else if (Settings.Default.overwriteLowerBitrate)
                        {
                            if (oldBitrate > oldTrack.BitRate)
                            {
                                oldTrack.Delete();
                                library.AddFile(songPath);
                                overWritten = true;
                            }
                        }
                    }

                    #endregion


                    #region Update UI
                    //create info string
                    string statusString = "";
                    if (duplicate) statusString = @"Duplicate";
                    else if (overWritten) statusString = @"Overwrote";
                    else statusString = @"Added";

                    //update UI
                    statusLabel.Invoke(new MethodInvoker(delegate { statusLabel.Text = statusString; }));


                    #endregion

                    
                    Directory.Delete(songFolder, true);
                    taggingInProgress = false;
                }
            }
        }

        private void button1_Click(object sender, EventArgs e)
        {
            started = !started;

            if (started)
            {
                downloadsWatcher.EnableRaisingEvents = true;
                songWatcher.EnableRaisingEvents = true;
                button1.Text = @"Stop";


                string dir = Directory.GetCurrentDirectory();
                string assnifferPath = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);
                if (Directory.Exists(assnifferPath))
                {
                    assnifferPath += @"\A Programmer's Crucible\iRadio Capture 2.0\assniffer\assniffer.exe";
                }
                else assnifferPath = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles) + @"\A Programmer's Crucible\iRadio Capture 2.0\assniffer\assniffer.exe";
                System.Diagnostics.ProcessStartInfo inf = new System.Diagnostics.ProcessStartInfo(assnifferPath, @""""+Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData)+@"\iRadio Capture 2.0"" -d "+Settings.Default.networkInterface+" -mimetype audio");
                inf.CreateNoWindow = true;
                inf.UseShellExecute = true;

                assnifferProcess = System.Diagnostics.Process.Start(inf);
                
                
            }

            else
            {
                button1.Text = @"Start";
                downloadsWatcher.EnableRaisingEvents = true;
                songWatcher.EnableRaisingEvents = true;
            }
        }

        private void updateUI(string[] tags, string imagePath)
        {
            if (songBuffering)
            {
                statusLabel.Invoke(new MethodInvoker(delegate { statusLabel.Text = @"Buffering"; }));

                string songInfoString;
                songInfoString = tags[0] + @" by " + tags[1] + @" on " + tags[2];

                //update UI
                if (pictureBox1.Image != null) pictureBox1.Image.Dispose();
                pictureBox1.Invoke(new MethodInvoker(delegate { pictureBox1.Image = Image.FromFile(imagePath); }));
                label1.Invoke(new MethodInvoker(delegate { label1.Text = songInfoString; }));

                if (previousImagePath != null) System.IO.File.Delete(previousImagePath);
                previousImagePath = imagePath;
            }

            else
            {
                statusLabel.Invoke(new MethodInvoker(delegate { statusLabel.Text = @"Error"; }));
            }
        }

        private void preferencesToolStripMenuItem_Click(object sender, EventArgs e)
        {
            SettingsForm settings = new SettingsForm();
            settings.Show();
        }

        
    }
}
