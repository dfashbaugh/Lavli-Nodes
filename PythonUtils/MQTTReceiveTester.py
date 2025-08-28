#!/usr/bin/env python3

import paho.mqtt.client as mqtt
import argparse
import time

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"Connected to MQTT broker with result code {rc}")
        client.subscribe(userdata['topic'])
        print(f"Subscribed to topic: {userdata['topic']}")
    else:
        print(f"Failed to connect to MQTT broker with result code {rc}")

def on_message(client, userdata, msg):
    print(f"Topic: {msg.topic}")
    print(f"Message: {msg.payload.decode()}")
    print("---")

def main():
    parser = argparse.ArgumentParser(description='MQTT Subscriber Tester')
    parser.add_argument('--server', '-s', required=True, help='MQTT broker server address')
    parser.add_argument('--port', '-p', type=int, default=1883, help='MQTT broker port (default: 1883)')
    parser.add_argument('--topic', '-t', required=True, help='Topic to subscribe to')
    parser.add_argument('--username', '-u', help='Username for authentication')
    parser.add_argument('--password', '-w', help='Password for authentication')
    
    args = parser.parse_args()
    
    client = mqtt.Client()
    client.user_data_set({'topic': args.topic})
    
    if args.username:
        client.username_pw_set(args.username, args.password)
    
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        print(f"Connecting to MQTT broker at {args.server}:{args.port}")
        client.tls_set()  # use system CA certificates
        client.connect(args.server, args.port, 60)
        
        print("Starting MQTT client loop. Press Ctrl+C to exit...")
        client.loop_forever()
        
    except KeyboardInterrupt:
        print("\nDisconnecting from MQTT broker...")
        client.disconnect()
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()