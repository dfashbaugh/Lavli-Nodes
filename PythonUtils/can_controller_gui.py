#!/usr/bin/env python3

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import paho.mqtt.client as mqtt
import json
import threading
import time
from datetime import datetime

class CANControllerGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Lavli CAN Controller")
        self.root.geometry("800x600")
        
        # MQTT client
        self.client = None
        self.connected = False
        
        # Create main interface
        self.create_widgets()
        
    def create_widgets(self):
        # Connection frame
        conn_frame = ttk.LabelFrame(self.root, text="MQTT Connection", padding="10")
        conn_frame.pack(fill="x", padx=10, pady=5)
        
        ttk.Label(conn_frame, text="Server:").grid(row=0, column=0, sticky="w", padx=5)
        self.server_var = tk.StringVar(value="b-f3e16066-c8b5-48a8-81b0-bc3347afef80-1.mq.us-east-1.amazonaws.com")
        ttk.Entry(conn_frame, textvariable=self.server_var, width=50).grid(row=0, column=1, padx=5)
        
        ttk.Label(conn_frame, text="Port:").grid(row=1, column=0, sticky="w", padx=5)
        self.port_var = tk.StringVar(value="8883")
        ttk.Entry(conn_frame, textvariable=self.port_var, width=10).grid(row=1, column=1, sticky="w", padx=5)
        
        ttk.Label(conn_frame, text="Username:").grid(row=2, column=0, sticky="w", padx=5)
        self.username_var = tk.StringVar(value="admin")
        ttk.Entry(conn_frame, textvariable=self.username_var, width=20).grid(row=2, column=1, sticky="w", padx=5)
        
        ttk.Label(conn_frame, text="Password:").grid(row=3, column=0, sticky="w", padx=5)
        self.password_var = tk.StringVar(value="lavlidevbroker!321")
        ttk.Entry(conn_frame, textvariable=self.password_var, width=20, show="*").grid(row=3, column=1, sticky="w", padx=5)
        
        self.connect_btn = ttk.Button(conn_frame, text="Connect", command=self.toggle_connection)
        self.connect_btn.grid(row=4, column=0, pady=10)
        
        self.status_label = ttk.Label(conn_frame, text="Disconnected", foreground="red")
        self.status_label.grid(row=4, column=1, sticky="w", padx=5)
        
        # Commands frame
        cmd_frame = ttk.LabelFrame(self.root, text="CAN Commands", padding="10")
        cmd_frame.pack(fill="both", expand=True, padx=10, pady=5)
        
        # Address input
        addr_frame = ttk.Frame(cmd_frame)
        addr_frame.pack(fill="x", pady=5)
        ttk.Label(addr_frame, text="CAN Address:").pack(side="left")
        self.address_var = tk.StringVar(value="0x311")
        ttk.Entry(addr_frame, textvariable=self.address_var, width=15).pack(side="left", padx=5)
        ttk.Label(addr_frame, text="(hex like 0x311 or decimal like 785)").pack(side="left", padx=5)
        
        # Command buttons frame
        btn_frame = ttk.Frame(cmd_frame)
        btn_frame.pack(fill="x", pady=10)
        
        # Motor commands
        motor_frame = ttk.LabelFrame(btn_frame, text="Motor Commands", padding="5")
        motor_frame.pack(side="left", fill="both", expand=True, padx=5)
        
        ttk.Button(motor_frame, text="Set Motor RPM", command=self.motor_rpm_dialog).pack(fill="x", pady=2)
        ttk.Button(motor_frame, text="Set Motor Direction", command=self.motor_direction_dialog).pack(fill="x", pady=2)
        ttk.Button(motor_frame, text="Stop Motor", command=self.motor_stop).pack(fill="x", pady=2)
        ttk.Button(motor_frame, text="Motor Status", command=self.motor_status).pack(fill="x", pady=2)
        
        # Output commands
        output_frame = ttk.LabelFrame(btn_frame, text="Output Commands", padding="5")
        output_frame.pack(side="left", fill="both", expand=True, padx=5)
        
        ttk.Button(output_frame, text="Activate Port", command=self.activate_port_dialog).pack(fill="x", pady=2)
        ttk.Button(output_frame, text="Deactivate Port", command=self.deactivate_port_dialog).pack(fill="x", pady=2)
        
        # Sensor commands
        sensor_frame = ttk.LabelFrame(btn_frame, text="Sensor Commands", padding="5")
        sensor_frame.pack(side="left", fill="both", expand=True, padx=5)
        
        ttk.Button(sensor_frame, text="Read Analog Pin", command=self.read_analog_dialog).pack(fill="x", pady=2)
        ttk.Button(sensor_frame, text="Read Digital Pin", command=self.read_digital_dialog).pack(fill="x", pady=2)
        ttk.Button(sensor_frame, text="Read All Analog", command=self.read_all_analog).pack(fill="x", pady=2)
        ttk.Button(sensor_frame, text="Read All Digital", command=self.read_all_digital).pack(fill="x", pady=2)
        
        # Custom command frame
        custom_frame = ttk.LabelFrame(cmd_frame, text="Custom Command", padding="5")
        custom_frame.pack(fill="x", pady=10)
        
        ttk.Label(custom_frame, text="Data (comma-separated):").pack(anchor="w")
        self.custom_data_var = tk.StringVar(value="0x01, 2")
        ttk.Entry(custom_frame, textvariable=self.custom_data_var, width=50).pack(fill="x", pady=2)
        ttk.Button(custom_frame, text="Send Custom Command", command=self.send_custom_command).pack(pady=5)
        
        # Log frame
        log_frame = ttk.LabelFrame(self.root, text="Log", padding="5")
        log_frame.pack(fill="both", expand=True, padx=10, pady=5)
        
        self.log_text = scrolledtext.ScrolledText(log_frame, height=8, state="disabled")
        self.log_text.pack(fill="both", expand=True)
        
        ttk.Button(log_frame, text="Clear Log", command=self.clear_log).pack(pady=5)
        
    def log_message(self, message):
        """Add a message to the log with timestamp"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        full_message = f"[{timestamp}] {message}\n"
        
        self.log_text.config(state="normal")
        self.log_text.insert(tk.END, full_message)
        self.log_text.see(tk.END)
        self.log_text.config(state="disabled")
        
    def clear_log(self):
        """Clear the log"""
        self.log_text.config(state="normal")
        self.log_text.delete(1.0, tk.END)
        self.log_text.config(state="disabled")
        
    def on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            self.connected = True
            self.root.after(0, lambda: self.update_connection_status(True))
            self.log_message("Connected to MQTT broker")
        else:
            self.connected = False
            self.root.after(0, lambda: self.update_connection_status(False))
            self.log_message(f"Failed to connect: {reason_code}")
            
    def on_publish(self, client, userdata, mid, reason_code, properties):
        self.log_message(f"Message published (ID: {mid})")
        
    def update_connection_status(self, connected):
        if connected:
            self.status_label.config(text="Connected", foreground="green")
            self.connect_btn.config(text="Disconnect")
        else:
            self.status_label.config(text="Disconnected", foreground="red")
            self.connect_btn.config(text="Connect")
            
    def toggle_connection(self):
        if self.connected:
            self.disconnect_mqtt()
        else:
            self.connect_mqtt()
            
    def connect_mqtt(self):
        try:
            self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
            self.client.username_pw_set(self.username_var.get(), self.password_var.get())
            self.client.on_connect = self.on_connect
            self.client.on_publish = self.on_publish
            self.client.tls_set()
            
            server = self.server_var.get()
            port = int(self.port_var.get())
            
            self.log_message(f"Connecting to {server}:{port}...")
            
            def connect_thread():
                try:
                    self.client.connect(server, port, 60)
                    self.client.loop_start()
                except Exception as e:
                    self.root.after(0, lambda: self.log_message(f"Connection error: {e}"))
                    
            threading.Thread(target=connect_thread, daemon=True).start()
            
        except Exception as e:
            messagebox.showerror("Connection Error", f"Failed to connect: {e}")
            
    def disconnect_mqtt(self):
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            self.connected = False
            self.update_connection_status(False)
            self.log_message("Disconnected from MQTT broker")
            
    def send_can_command(self, address, data_list):
        """Send a CAN command via MQTT"""
        if not self.connected:
            messagebox.showerror("Error", "Not connected to MQTT broker")
            return
            
        try:
            # Parse address
            if isinstance(address, str) and (address.startswith('0x') or address.startswith('0X')):
                addr_val = address
            else:
                addr_val = address
                
            # Create JSON command
            command = {
                "address": addr_val,
                "data": data_list
            }
            
            json_str = json.dumps(command)
            
            self.log_message(f"Sending: {json_str}")
            
            result = self.client.publish("/lavli/can", json_str, qos=0)
            
            if result.rc != mqtt.MQTT_ERR_SUCCESS:
                self.log_message(f"Failed to publish: {result.rc}")
                
        except Exception as e:
            messagebox.showerror("Error", f"Failed to send command: {e}")
            
    def motor_rpm_dialog(self):
        """Dialog for setting motor RPM"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Set Motor RPM")
        dialog.geometry("300x150")
        dialog.transient(self.root)
        dialog.grab_set()
        
        ttk.Label(dialog, text="RPM (0-1500):").pack(pady=10)
        rpm_var = tk.StringVar(value="100")
        ttk.Entry(dialog, textvariable=rpm_var, width=10).pack(pady=5)
        
        def send_rpm():
            try:
                rpm = int(rpm_var.get())
                if 0 <= rpm <= 1500:
                    address = self.address_var.get()
                    self.send_can_command(address, ["0x30", rpm >> 8, rpm & 0xFF])
                    dialog.destroy()
                else:
                    messagebox.showerror("Error", "RPM must be between 0 and 1500")
            except ValueError:
                messagebox.showerror("Error", "Please enter a valid number")
                
        ttk.Button(dialog, text="Send", command=send_rpm).pack(pady=10)
        
    def motor_direction_dialog(self):
        """Dialog for setting motor direction"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Set Motor Direction")
        dialog.geometry("300x150")
        dialog.transient(self.root)
        dialog.grab_set()
        
        direction_var = tk.StringVar(value="1")
        ttk.Radiobutton(dialog, text="Clockwise", variable=direction_var, value="1").pack(pady=10)
        ttk.Radiobutton(dialog, text="Counter-Clockwise", variable=direction_var, value="0").pack(pady=5)
        
        def send_direction():
            address = self.address_var.get()
            direction = int(direction_var.get())
            self.send_can_command(address, ["0x31", direction])
            dialog.destroy()
            
        ttk.Button(dialog, text="Send", command=send_direction).pack(pady=10)
        
    def motor_stop(self):
        """Send motor stop command"""
        address = self.address_var.get()
        self.send_can_command(address, ["0x32"])
        
    def motor_status(self):
        """Request motor status"""
        address = self.address_var.get()
        self.send_can_command(address, ["0x33"])
        
    def activate_port_dialog(self):
        """Dialog for activating a port"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Activate Port")
        dialog.geometry("300x150")
        dialog.transient(self.root)
        dialog.grab_set()
        
        ttk.Label(dialog, text="Port Number (0-7):").pack(pady=10)
        port_var = tk.StringVar(value="0")
        ttk.Entry(dialog, textvariable=port_var, width=10).pack(pady=5)
        
        def send_activate():
            try:
                port = int(port_var.get())
                if 0 <= port <= 7:
                    address = self.address_var.get()
                    self.send_can_command(address, ["0x01", port])
                    dialog.destroy()
                else:
                    messagebox.showerror("Error", "Port must be between 0 and 7")
            except ValueError:
                messagebox.showerror("Error", "Please enter a valid port number")
                
        ttk.Button(dialog, text="Activate", command=send_activate).pack(pady=10)
        
    def deactivate_port_dialog(self):
        """Dialog for deactivating a port"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Deactivate Port")
        dialog.geometry("300x150")
        dialog.transient(self.root)
        dialog.grab_set()
        
        ttk.Label(dialog, text="Port Number (0-7):").pack(pady=10)
        port_var = tk.StringVar(value="0")
        ttk.Entry(dialog, textvariable=port_var, width=10).pack(pady=5)
        
        def send_deactivate():
            try:
                port = int(port_var.get())
                if 0 <= port <= 7:
                    address = self.address_var.get()
                    self.send_can_command(address, ["0x02", port])
                    dialog.destroy()
                else:
                    messagebox.showerror("Error", "Port must be between 0 and 7")
            except ValueError:
                messagebox.showerror("Error", "Please enter a valid port number")
                
        ttk.Button(dialog, text="Deactivate", command=send_deactivate).pack(pady=10)
        
    def read_analog_dialog(self):
        """Dialog for reading analog pin"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Read Analog Pin")
        dialog.geometry("300x150")
        dialog.transient(self.root)
        dialog.grab_set()
        
        ttk.Label(dialog, text="Pin Number (0-7):").pack(pady=10)
        pin_var = tk.StringVar(value="0")
        ttk.Entry(dialog, textvariable=pin_var, width=10).pack(pady=5)
        
        def send_read():
            try:
                pin = int(pin_var.get())
                if 0 <= pin <= 7:
                    address = self.address_var.get()
                    self.send_can_command(address, ["0x03", pin])
                    dialog.destroy()
                else:
                    messagebox.showerror("Error", "Pin must be between 0 and 7")
            except ValueError:
                messagebox.showerror("Error", "Please enter a valid pin number")
                
        ttk.Button(dialog, text="Read", command=send_read).pack(pady=10)
        
    def read_digital_dialog(self):
        """Dialog for reading digital pin"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Read Digital Pin")
        dialog.geometry("300x150")
        dialog.transient(self.root)
        dialog.grab_set()
        
        ttk.Label(dialog, text="Pin Number (0-7):").pack(pady=10)
        pin_var = tk.StringVar(value="0")
        ttk.Entry(dialog, textvariable=pin_var, width=10).pack(pady=5)
        
        def send_read():
            try:
                pin = int(pin_var.get())
                if 0 <= pin <= 7:
                    address = self.address_var.get()
                    self.send_can_command(address, ["0x04", pin])
                    dialog.destroy()
                else:
                    messagebox.showerror("Error", "Pin must be between 0 and 7")
            except ValueError:
                messagebox.showerror("Error", "Please enter a valid pin number")
                
        ttk.Button(dialog, text="Read", command=send_read).pack(pady=10)
        
    def read_all_analog(self):
        """Read all analog pins"""
        address = self.address_var.get()
        self.send_can_command(address, ["0x05"])
        
    def read_all_digital(self):
        """Read all digital pins"""
        address = self.address_var.get()
        self.send_can_command(address, ["0x06"])
        
    def send_custom_command(self):
        """Send custom command from user input"""
        try:
            address = self.address_var.get()
            data_str = self.custom_data_var.get()
            
            # Parse comma-separated data
            data_parts = [part.strip() for part in data_str.split(',')]
            data_list = []
            
            for part in data_parts:
                if part.startswith('0x') or part.startswith('0X'):
                    data_list.append(part)
                else:
                    data_list.append(int(part))
                    
            self.send_can_command(address, data_list)
            
        except Exception as e:
            messagebox.showerror("Error", f"Invalid data format: {e}")

def main():
    root = tk.Tk()
    app = CANControllerGUI(root)
    
    def on_closing():
        app.disconnect_mqtt()
        root.destroy()
        
    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()

if __name__ == "__main__":
    main()