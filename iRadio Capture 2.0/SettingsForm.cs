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
using iRadio_Capture_2._0.Properties;

namespace iRadio_Capture_2._0
{
    public partial class SettingsForm : Form
    {
        public SettingsForm()
        {
            InitializeComponent();
            if (Settings.Default.customFolder)
            {
                radioButton1.Select();
            }

            else
            {
                radioButton2.Select();
                textBox2.Enabled = checkBox2.Checked;
            }

            textBox3.Text = Settings.Default.networkInterface.ToString();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            Settings.Default.customFolder = radioButton1.Checked;
            Settings.Default.networkInterface = Convert.ToInt16(textBox3.Text);
            Settings.Default.Save();
            this.Close();
            
        }

        private void button3_Click(object sender, EventArgs e)
        {
            FolderBrowserDialog diag = new FolderBrowserDialog();

            if (diag.ShowDialog() == DialogResult.OK)
            {
                textBox1.Text = diag.SelectedPath;
            }
        }

        private void button2_Click(object sender, EventArgs e)
        {
            Settings.Default.Reload();
            this.Close();
        }

        private void checkBox2_CheckedChanged(object sender, EventArgs e)
        {
            CheckBox box = (CheckBox)sender;
            textBox2.Enabled = box.Checked;
        }

        private void radioButton2_CheckedChanged(object sender, EventArgs e)
        {
            RadioButton button = (RadioButton)sender;
            textBox2.Enabled = button.Checked & checkBox2.Checked;
            checkBox2.Enabled = button.Checked;
        }
    }
}
