import gi
import os
import shutil
import subprocess
from urllib.parse import urlparse

gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, GObject, Pango, Gdk

# Load custom CSS to set typography according to Material Design guidelines,
# without overriding GNOME theme colours.
css = b"""
.label {
    font-family: 'Roboto', sans-serif;
    font-size: 16px;
    margin-bottom: 8px;
}
.entry {
    font-family: 'Roboto', sans-serif;
    font-size: 16px;
}
"""

provider = Gtk.CssProvider()
provider.load_from_data(css)
screen = Gdk.Screen.get_default()
Gtk.StyleContext.add_provider_for_screen(screen, provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

class WavyApp(Gtk.Window):
    def __init__(self):
        super().__init__(title="Wavy")
        self.set_default_size(500, 400)

        self.stack = Gtk.Stack()
        self.add(self.stack)

        # Page 1: File Selection
        self.file_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=20)
        self.file_box.set_border_width(30)
        
        self.file_label = Gtk.Label(label="Select File to Stream.")
        self.file_label.set_xalign(0)
        
        self.select_button = Gtk.Button(label="Select...")
        self.select_button.connect("clicked", self.on_select_file)
        
        self.file_box.pack_start(self.file_label, False, False, 0)
        self.file_box.pack_start(self.select_button, False, False, 0)
        
        self.stack.add_named(self.file_box, "file_selection")

        # Page 2: Directory Selection
        self.dir_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=20)
        self.dir_box.set_border_width(30)
        
        self.dir_label = Gtk.Label(label="Select Output Directory")
        self.dir_label.set_xalign(0)
        
        self.dir_select_button = Gtk.Button(label="Choose Folder...")
        self.dir_select_button.connect("clicked", self.on_select_directory)
        
        self.dir_warning_label = Gtk.Label()  # For warnings if directory is not empty.
        self.dir_warning_label.set_xalign(0)
        self.dir_warning_label.set_markup("<span foreground='red'></span>")
        
        self.dir_box.pack_start(self.dir_label, False, False, 0)
        self.dir_box.pack_start(self.dir_select_button, False, False, 0)
        self.dir_box.pack_start(self.dir_warning_label, False, False, 0)
        
        self.stack.add_named(self.dir_box, "directory_selection")

        # Page 3: URL Input
        self.url_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=20)
        self.url_box.set_border_width(30)

        self.url_title = Gtk.Label(label="Enter URL of the Server")
        self.url_title.set_xalign(0)
        
        # File display with a frame (uses default theme colours)
        self.file_frame = Gtk.Frame()
        self.file_frame.set_shadow_type(Gtk.ShadowType.IN)
        self.file_display = Gtk.Label()
        self.file_frame.add(self.file_display)
        
        # Directory display with a frame (uses default theme colours)
        self.dir_frame = Gtk.Frame()
        self.dir_frame.set_shadow_type(Gtk.ShadowType.IN)
        self.dir_display = Gtk.Label()
        self.dir_frame.add(self.dir_display)

        entry_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        self.entry = Gtk.Entry()
        self.entry.set_placeholder_text("Server URL (with port)")
        self.entry.connect("activate", self.on_forward)

        self.forward_button = Gtk.Button(label="Enter")
        self.forward_button.connect("clicked", self.on_forward)

        entry_box.pack_start(self.entry, True, True, 0)
        entry_box.pack_start(self.forward_button, False, False, 0)
        
        self.url_box.pack_start(self.url_title, False, False, 0)
        self.url_box.pack_start(self.file_frame, False, False, 0)
        self.url_box.pack_start(self.dir_frame, False, False, 0)
        self.url_box.pack_start(entry_box, False, False, 0)
        
        self.stack.add_named(self.url_box, "url_input")

        # Page 4: Log Output
        self.log_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.log_box.set_border_width(30)
        
        self.log_view = Gtk.TextView()
        self.log_view.set_editable(False)
        self.log_view.set_cursor_visible(False)
        self.log_view.set_wrap_mode(Gtk.WrapMode.WORD)
        self.log_view.modify_font(Pango.FontDescription("monospace"))
        self.log_buffer = self.log_view.get_buffer()
        
        log_scroll = Gtk.ScrolledWindow()
        log_scroll.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
        log_scroll.add(self.log_view)
        
        self.log_box.pack_start(log_scroll, True, True, 0)
        
        self.stack.add_named(self.log_box, "log_output")
        
        self.stack.set_visible_child_name("file_selection")
    
    def on_select_file(self, widget):
        dialog = Gtk.FileChooserDialog(
            title="Select an MP3 or FLAC file", parent=self, action=Gtk.FileChooserAction.OPEN,
            buttons=(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL, Gtk.STOCK_OPEN, Gtk.ResponseType.OK)
        )
        file_filter = Gtk.FileFilter()
        file_filter.set_name("Audio Files")
        file_filter.add_pattern("*.mp3")
        file_filter.add_pattern("*.flac")
        dialog.add_filter(file_filter)
        
        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            self.selected_file = dialog.get_filename()
            self.file_display.set_text(self.selected_file)
            self.stack.set_visible_child_name("directory_selection")
        dialog.destroy()
    
    def on_select_directory(self, widget):
        dialog = Gtk.FileChooserDialog(
            title="Select Output Directory", parent=self, action=Gtk.FileChooserAction.SELECT_FOLDER,
            buttons=(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL, Gtk.STOCK_OPEN, Gtk.ResponseType.OK)
        )
        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            self.selected_directory = dialog.get_filename()
            self.dir_display.set_text(self.selected_directory)
            # Check if directory is not empty and show warning if needed.
            if os.listdir(self.selected_directory):
                self.dir_warning_label.set_markup("<span foreground='red'>Warning: Directory is not empty. Contents will be deleted.</span>")
            else:
                self.dir_warning_label.set_text("")  # Clear warning if empty.
            self.stack.set_visible_child_name("url_input")
        dialog.destroy()
    
    def on_forward(self, widget):
        url = self.entry.get_text().strip()
        # Prepend http:// if missing
        if not (url.startswith("http://") or url.startswith("https://")):
            url = "http://" + url
        
        # Validate URL: must include a port
        from urllib.parse import urlparse
        parsed = urlparse(url)
        if parsed.port is None:
            self.show_error("URL must include a port number (e.g., http://example.com:8080).")
            return
        
        self.entry.set_text(url)  # update the entry field with the full URL

        # If the directory is not empty, show a disclaimer before deletion.
        if os.listdir(self.selected_directory):
            disclaimer_dialog = Gtk.MessageDialog(parent=self, flags=0,
                                                  message_type=Gtk.MessageType.WARNING,
                                                  buttons=Gtk.ButtonsType.YES_NO,
                                                  text="Warning: The selected directory is not empty. All contents will be deleted.\nDo you want to proceed?")
            response = disclaimer_dialog.run()
            disclaimer_dialog.destroy()
            if response != Gtk.ResponseType.YES:
                return
            else:
                self.delete_directory_contents(self.selected_directory)
        
        self.stack.set_visible_child_name("log_output")
        self.run_subprocess(self.selected_file, self.selected_directory, url)
    
    def delete_directory_contents(self, directory):
        for filename in os.listdir(directory):
            file_path = os.path.join(directory, filename)
            try:
                if os.path.isfile(file_path) or os.path.islink(file_path):
                    os.remove(file_path)
                elif os.path.isdir(file_path):
                    shutil.rmtree(file_path)
            except Exception as e:
                self.append_log(f"Failed to delete {file_path}: {e}")
    
    def run_subprocess(self, file, directory, url):
        # Replace "your_program" with the actual executable/command you want to run.
        command = ["./build/hls_segmenter", file, directory, file.split(".")[-1]]
        addr, port = url.rsplit(":", 1)
        dispatcher = ["./build/hls_dispatcher", addr.split("//", 1)[1], port, directory, "index.m3u8"]
        print(dispatcher)
        try:
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            process2 = subprocess.Popen(dispatcher, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        except Exception as e:
            self.append_log(f"Failed to start subprocess: {e}")
            return
        
        def read_output():
            for line in process2.stdout:
                GObject.idle_add(self.append_log, line.rstrip())
        GObject.idle_add(read_output)
    
    def append_log(self, text):
        end_iter = self.log_buffer.get_end_iter()
        self.log_buffer.insert(end_iter, text + "\n")
        return False
    
    def show_error(self, message):
        dialog = Gtk.MessageDialog(parent=self, flags=0,
                                   message_type=Gtk.MessageType.ERROR,
                                   buttons=Gtk.ButtonsType.OK,
                                   text=message)
        dialog.run()
        dialog.destroy()

app = WavyApp()
app.connect("destroy", Gtk.main_quit)
app.show_all()
Gtk.main()
