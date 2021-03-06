#include <LSM303.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <math.h>
#include <WiFi101.h>
#include "secrets.h"


#define MOTOR1_PINA 10
#define MOTOR1_PINB 11
#define MOTOR2_PINA 9 
#define MOTOR2_PINB 5 

#define angleTolerance 10 //+- degrees
#define rotate_speed 0.3

#define Kp  2

#define pi 3.14159


LSM303 imu;

char ssid[] = SECRET_SSID;  // network SSID)
char pass[] = SECRET_PASS;  //network password

int status = WL_IDLE_STATUS; 
WiFiServer server(80); 
WiFiUDP Udp; 
double udpPacketDelay = (1/returnRate)*1000; //convert to milliseconds

double tick = 0; 
double tock = 0; 

values desired; 
values actual; 

void setup() {
  Serial.begin(9600);
  motorsetup();
  imusetup(); 
  APsetup(); 
  tick = millis();
  tock = millis(); 
  desired.velocity = 0; 
  desired.theta = 0;  
  desired.rst = 0;
  actual.velocity = 0; 
  actual.theta = 0; 
  actual.rst = 0; 

}

void imusetup(){
  Wire.begin();
  imu.init();
  imu.enableDefault();
  imu.read();
  //min and max values gotten from calibrate example for lsm303d
  //always run calibrate imu program and update below with observed values
  imu.m_min = (LSM303::vector<int16_t>){-2985, -2841, -7073};
  imu.m_max = (LSM303::vector<int16_t>){+2870, +4579, +241};
  moveToAngle(10); //orient close to North
}

void setNetwork(){
  IPAddress ip = {  NETWORK_IP_1,
                    NETWORK_IP_2,
                    NETWORK_IP_3,
                    NETWORK_IP_4
                  };
                  
  IPAddress dns = { NETWORK_IP_1,
                    NETWORK_IP_2,
                    NETWORK_IP_3,
                    NETWORK_IP_4
                  };
                  
  IPAddress gateway = { NETWORK_IP_1,
                        NETWORK_IP_2,
                        NETWORK_IP_3,
                        NETWORK_IP_4
                      };
  
  IPAddress subnet = {  NETMASK_1,
                        NETMASK_2,
                        NETMASK_3,
                        NETMASK_4
                       };

  WiFi.config(ip, dns, gateway, subnet);  

}

void APsetup(){
  WiFi.setPins(8, 7, 4, 2);
  setNetwork();
  while(!Serial);
  if(WiFi.status() == WL_NO_SHIELD){
    //stop program
    while(true);
  }

  // by default the local IP address of will be 192.168.1.1
  // you can override it with the following:
  //WiFi.config(IPAddress(10, 0, 0, 30));
  
  // print the network name (SSID);
  Serial.print("Creating access point named: ");
  Serial.println(ssid);

  status = WiFi.beginAP(ssid);
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    // don't continue
    while (true);
  }
  //wait 3 seconds for connection
  delay(3000);
  //start web server on port 80
  printWiFiStatus(); 
  server.begin();
  Serial.print("\nListening for UDP packets on port ");
  Serial.println(UDP_PORT_LISTEN); 
  Serial.print("Result of UDP.begin ");
  Serial.println(Udp.begin(UDP_PORT_LISTEN));
}

void motorsetup(){
  pinMode(MOTOR1_PINA, OUTPUT); 
  pinMode(MOTOR1_PINB, OUTPUT);
  pinMode(MOTOR2_PINA, OUTPUT); 
  pinMode(MOTOR2_PINB, OUTPUT); 
  analogWrite(MOTOR1_PINA, 0);
  analogWrite(MOTOR1_PINA, 0);
  analogWrite(MOTOR2_PINB, 0);
  analogWrite(MOTOR2_PINB, 0);
}

int setSpeed(float s){
  if(s > 1.0) s = 1.0; 
  if(s < 0.0) s = 0.0; 
  int out = (int) (s*255.0); 
  return out; 
}

void setAngularVelocity(float velocity){
  float s = velocity; 
  if(s < 0) s = -s; 
  int sp1 = setSpeed(s);  //speed motor 1
  int sp2 = setSpeed(s);  //speed motor 2
  if(velocity < 0){ //turn clockwise  
    analogWrite(MOTOR1_PINB, 0);
    analogWrite(MOTOR1_PINA, sp1);
    analogWrite(MOTOR2_PINB, 0);
    analogWrite(MOTOR2_PINA, sp2);
  }
  else { //turn counterclockwise
    analogWrite(MOTOR1_PINA, 0);
    analogWrite(MOTOR1_PINB, sp1);
    analogWrite(MOTOR2_PINA, 0);
    analogWrite(MOTOR2_PINB, sp2);
  }
}

void motorsOff(){
  analogWrite(MOTOR1_PINA, 0);
  analogWrite(MOTOR1_PINB, 0);
  analogWrite(MOTOR2_PINA, 0);
  analogWrite(MOTOR2_PINB, 0);
}

void moveToAngle(int targetAngle){ //angle should be given in degrees 
  imu.read(); 
  actual.theta = imu.heading(); 
  double error = actual.theta-targetAngle; //if negative rotate clockwise 
  error = (int) error % 360; // %make error between 0 (inclusive) and 360.0 (exclusive)
  
  if(abs(error) > angleTolerance){
    
    if(error > 0){//clockwise  
      setAngularVelocity(rotate_speed);
    }
    else{ //clockwise
      setAngularVelocity(-rotate_speed);
    }
    error = actual.theta-targetAngle; 
    error = (int) error % 360; // %make error between 0 (inclusive) and 360.0 (exclusive)
    imu.read();
  }
 motorsOff(); 
  
}

void readUDP(){
  int packetSize = Udp.parsePacket();
  if (packetSize)
    {
      udp_recv udp; 
      memset(&udp, 0, sizeof(udp));
      // read the packet into packetBufffer
      int len = Udp.read((byte *) &udp, 255);
      desired.theta = udp.theta;
      desired.velocity = udp.velocity;
      desired.rst = udp.rst; 
  }
}

void sendUDP(){
  //Serial.println("Constructing and sending UDP packet");
  udp_send udp; 
  IPAddress targetIP = {  
                        DESTINATION_OCT_1, 
                        DESTINATION_OCT_2, 
                        DESTINATION_OCT_3, 
                        DESTINATION_OCT_4
                        };
                      
  memset(&udp, 0, sizeof(udp));
  imu.read(); 
  char testBuffer[] = "hello"; 
  udp.imu[0] = (double) imu.a.x;
  udp.imu[1] = (double) imu.a.y;
  udp.imu[2] = (double) imu.a.z;
  udp.imu[3] = (double) imu.m.x;
  udp.imu[4] = (double) imu.m.y;
  udp.imu[5] = (double) imu.m.z;
  udp.odo[0] = 0; 
  udp.odo[1] = 0; 
  udp.odo[2] = 0; 
  udp.heading = (double) imu.heading(); 
  //char* outgoing = (char *) &udp; 
  Udp.beginPacket(targetIP, UDP_PORT_SEND);
  Udp.write((char *) &udp, sizeof(udp));
  //Udp.write(testBuffer);
  Udp.endPacket(); 
 
}

void loop() {
  // put your main code here, to run repeatedly:
  readUDP(); 
  if(tick - tock > udpPacketDelay){
    sendUDP(); 
    tock = millis(); 
  }
  moveToAngle(desired.theta);
  tick = millis(); 
}


void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  // print where to go in a browser:
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);

}
