import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GLib, Pango, Gdk
import subprocess
import threading
import os

class HLSClientWindow(Gtk.Window):
    def __init__(self):
        Gtk.Window.__init__(self, title="Wavy HLS Client")
        self.set_default_size(500, 300)
        self.set_border_width(20)
        
        # Main container
        main_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=20)
        self.add(main_box)
        
        # Page title
        title_label = Gtk.Label()
        title_label.set_markup("<span font='16pt' weight='bold'>Wavy Client</span>")
        title_label.set_margin_bottom(20)
        main_box.pack_start(title_label, False, False, 0)

        # Centered content container
        content_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=15)
        content_box.set_halign(Gtk.Align.CENTER)
        content_box.set_valign(Gtk.Align.CENTER)
        content_box.set_hexpand(True)
        content_box.set_vexpand(True)
        main_box.pack_start(content_box, True, True, 0)

        # Form grid
        form_grid = Gtk.Grid(column_spacing=15, row_spacing=10)
        form_grid.set_halign(Gtk.Align.CENTER)
        
        # Server IP
        server_label = Gtk.Label(label="Server IP:")
        server_label.set_halign(Gtk.Align.END)
        self.server_entry = Gtk.Entry(width_chars=25)
        self.server_entry.set_placeholder_text("Enter server IP address")
        
        # Client Index
        index_label = Gtk.Label(label="Client Index:")
        index_label.set_halign(Gtk.Align.END)
        self.index_entry = Gtk.Entry(width_chars=5)
        self.index_entry.set_placeholder_text("0")
        
        # Add to form grid
        form_grid.attach(server_label, 0, 0, 1, 1)
        form_grid.attach(self.server_entry, 1, 0, 1, 1)
        form_grid.attach(index_label, 0, 1, 1, 1)
        form_grid.attach(self.index_entry, 1, 1, 1, 1)
        
        content_box.pack_start(form_grid, False, False, 0)

        # Control buttons
        button_box = Gtk.Box(spacing=15)
        button_box.set_halign(Gtk.Align.CENTER)
        
        self.play_button = Gtk.Button(label="▶ Play")
        self.play_button.get_style_context().add_class("suggested-action")
        self.play_button.set_size_request(100, 40)
        
        self.stop_button = Gtk.Button(label="⏹ Stop")
        self.stop_button.set_size_request(100, 40)
        self.stop_button.set_sensitive(False)
        
        button_box.pack_start(self.play_button, False, False, 0)
        button_box.pack_start(self.stop_button, False, False, 0)
        content_box.pack_start(button_box, False, False, 0)

        # Status label
        self.status_label = Gtk.Label()
        self.status_label.set_halign(Gtk.Align.CENTER)
        self.status_label.set_ellipsize(Pango.EllipsizeMode.MIDDLE)
        self.update_status("Ready to connect")
        content_box.pack_start(self.status_label, False, False, 10)

        # Signal connections
        self.play_button.connect("clicked", self.on_play_clicked)
        self.stop_button.connect("clicked", self.on_stop_clicked)
        self.playback_process = None

    def update_status(self, message, is_error=False):
        style = "<span font='10pt' style='italic' foreground='%s'>%s</span>"
        color = "#FF4444" if is_error else "#666666"
        self.status_label.set_markup(style % (color, message))

    # Rest of the functionality remains the same
    def on_play_clicked(self, button):
        server_ip = self.server_entry.get_text()
        client_index = self.index_entry.get_text()
        
        if not server_ip or not client_index:
            self.update_status("Please fill in all fields", is_error=True)
            return
            
        if not client_index.isdigit():
            self.update_status("Client index must be a number", is_error=True)
            return
            
        self.play_button.set_sensitive(False)
        self.stop_button.set_sensitive(True)
        self.update_status("Connecting to server...")
        
        threading.Thread(
            target=self.run_client,
            args=(client_index, server_ip),
            daemon=True
        ).start()

    def run_client(self, index, server_ip):
        try:
            client_path = "./build/hls_client"
            if not os.path.exists(client_path):
                GLib.idle_add(self.update_status, "Client binary not found!", True)
                return
            
            self.playback_process = subprocess.Popen(
                [client_path, index, server_ip],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            
            GLib.idle_add(self.update_status, "Playing audio stream...")
            self.playback_process.wait()
            
            if self.playback_process.returncode == 0:
                GLib.idle_add(self.update_status, "Playback completed")
            else:
                err = self.playback_process.stderr.read().decode()
                GLib.idle_add(self.update_status, f"Error: {err}", True)
            
        except Exception as e:
            GLib.idle_add(self.update_status, f"Error: {str(e)}", True)
        finally:
            GLib.idle_add(self.reset_ui)

    def on_stop_clicked(self, button):
        if self.playback_process:
            self.playback_process.terminate()
            self.update_status("Playback stopped")
        self.reset_ui()

    def reset_ui(self):
        self.play_button.set_sensitive(True)
        self.stop_button.set_sensitive(False)
        self.playback_process = None

# Add some basic CSS styling
css = b"""
button.suggested-action {
    background-color: #4CAF50;
    color: white;
}
"""
style_provider = Gtk.CssProvider()
style_provider.load_from_data(css)

Gtk.StyleContext.add_provider_for_screen(
    Gdk.Screen.get_default(),
    style_provider,
    Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
)

win = HLSClientWindow()
win.connect("destroy", Gtk.main_quit)
win.show_all()
Gtk.main()
