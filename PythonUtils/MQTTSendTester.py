#!/usr/bin/env python3

import paho.mqtt.client as mqtt
import argparse
import time
import json

def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print(f"Connected to MQTT broker with result code {reason_code}")
    else:
        print(f"Failed to connect to MQTT broker with result code {reason_code}")

def on_publish(client, userdata, mid, reason_code, properties):
    print(f"Message published successfully (message ID: {mid})")

def main():
    parser = argparse.ArgumentParser(description='MQTT Publisher Tester')
    parser.add_argument('--server', '-s', required=True, help='MQTT broker server address')
    parser.add_argument('--port', '-p', type=int, default=1883, help='MQTT broker port (default: 1883)')
    parser.add_argument('--topic', '-t', required=True, help='Topic to publish to')
    parser.add_argument('--message', '-m', required=True, help='Message to publish')
    parser.add_argument('--username', '-u', help='Username for authentication')
    parser.add_argument('--password', '-w', help='Password for authentication')
    parser.add_argument('--qos', '-q', type=int, default=0, choices=[0, 1, 2], help='QoS level (default: 0)')
    parser.add_argument('--retain', '-r', action='store_true', help='Retain message')
    parser.add_argument('--repeat', type=int, default=1, help='Number of times to send the message (default: 1)')
    parser.add_argument('--interval', type=float, default=1.0, help='Interval between messages in seconds (default: 1.0)')
    
    args = parser.parse_args()
    
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    
    if args.username:
        client.username_pw_set(args.username, args.password)
    
    client.on_connect = on_connect
    client.on_publish = on_publish
    
    try:
        print(f"Connecting to MQTT broker at {args.server}:{args.port}")
        client.tls_set()  # use system CA certificates
        client.connect(args.server, args.port, 60)
        client.loop_start()
        
        time.sleep(1)
        
        for i in range(args.repeat):
            if args.repeat > 1:
                message = f"{args.message} (#{i+1})"
            else:
                message = args.message
                
            print(f"Publishing to topic '{args.topic}': {message}")
            result = client.publish(args.topic, message, qos=args.qos, retain=args.retain)
            
            if result.rc != mqtt.MQTT_ERR_SUCCESS:
                print(f"Failed to publish message: {result.rc}")
            
            if i < args.repeat - 1:
                time.sleep(args.interval)
        
        time.sleep(1)
        client.loop_stop()
        client.disconnect()
        print("Disconnected from MQTT broker")
        
    except KeyboardInterrupt:
        print("\nDisconnecting from MQTT broker...")
        client.loop_stop()
        client.disconnect()
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()