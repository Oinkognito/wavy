import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GLib
import subprocess
import threading
import os

class HLSClientWindow(Gtk.Window):
    def __init__(self):
        Gtk.Window.__init__(self, title="Wavy HLS Client")
        self.set_default_size(400, 200)
        
        # Create UI elements
        
        self.grid = Gtk.Grid(column_spacing=10, row_spacing=10)
        self.grid.set_margin_start(10)    # Left margin
        self.grid.set_margin_end(10)      # Right margin
        self.grid.set_margin_top(10)      # Top margin
        self.grid.set_margin_bottom(10)   # Bottom margin

        self.server_label = Gtk.Label(label="Server IP:")
        self.server_entry = Gtk.Entry()
        self.index_label = Gtk.Label(label="Client Index:")
        self.index_entry = Gtk.Entry()
        
        self.play_button = Gtk.Button(label="Play")
        self.stop_button = Gtk.Button(label="Stop")
        self.status_label = Gtk.Label(label="Status: Ready")
        
        # Set up grid
        self.grid.attach(self.server_label, 0, 0, 1, 1)
        self.grid.attach(self.server_entry, 1, 0, 2, 1)
        self.grid.attach(self.index_label, 0, 1, 1, 1)
        self.grid.attach(self.index_entry, 1, 1, 2, 1)
        self.grid.attach(self.play_button, 0, 2, 1, 1)
        self.grid.attach(self.stop_button, 1, 2, 1, 1)
        self.grid.attach(self.status_label, 0, 3, 3, 1)
        
        self.add(self.grid)
        
        # Initial state
        self.stop_button.set_sensitive(False)
        self.playback_process = None
        
        # Connect signals
        self.play_button.connect("clicked", self.on_play_clicked)
        self.stop_button.connect("clicked", self.on_stop_clicked)

    def on_play_clicked(self, button):
        server_ip = self.server_entry.get_text()
        client_index = self.index_entry.get_text()
        
        if not server_ip or not client_index:
            self.status_label.set_label("Status: Missing server IP or client index!")
            return
            
        if not client_index.isdigit():
            self.status_label.set_label("Status: Client index must be a number!")
            return
            
        self.play_button.set_sensitive(False)
        self.stop_button.set_sensitive(True)
        self.status_label.set_label("Status: Connecting...")
        
        # Start playback in background thread
        threading.Thread(
            target=self.run_client,
            args=(client_index, server_ip),
            daemon=True
        ).start()

    def run_client(self, index, server_ip):
        try:
            client_path = os.path.abspath("./build/hls_client")
            if not os.path.exists(client_path):
                GLib.idle_add(self.status_label.set_label, "Status: Client binary not found!")
                return
            
            self.playback_process = subprocess.Popen(
                [client_path, index, server_ip],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            
            GLib.idle_add(self.status_label.set_label, "Status: Playing...")
            
            # Wait for completion
            self.playback_process.wait()
            
            if self.playback_process.returncode == 0:
                GLib.idle_add(self.status_label.set_label, "Status: Playback completed")
            else:
                err = self.playback_process.stderr.read().decode()
                GLib.idle_add(self.status_label.set_label, f"Status: Error: {err}")
            
        except Exception as e:
            GLib.idle_add(self.status_label.set_label, f"Status: Error: {str(e)}")
        finally:
            GLib.idle_add(self.reset_ui)

    def on_stop_clicked(self, button):
        if self.playback_process:
            self.playback_process.terminate()
            self.status_label.set_label("Status: Stopped")
        self.reset_ui()

    def reset_ui(self):
        self.play_button.set_sensitive(True)
        self.stop_button.set_sensitive(False)
        self.playback_process = None

win = HLSClientWindow()
win.connect("destroy", Gtk.main_quit)
win.show_all()
Gtk.main()
