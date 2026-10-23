int rs=8,en=9,d4=4,d5=5,d6=6,d7=7;

int count=1;
int maxcount=2000;   

void pulse(){
 digitalWrite(en,1);
 delayMicroseconds(1);
 digitalWrite(en,0);
 delayMicroseconds(150);
}

void send4(int x){
 digitalWrite(d4,(x>>0)&1);
 digitalWrite(d5,(x>>1)&1);
 digitalWrite(d6,(x>>2)&1);
 digitalWrite(d7,(x>>3)&1);
 pulse();
}

void cmd(int x){
 digitalWrite(rs,0);
 send4(x>>4);
 send4(x);
}

void data(char x){
 digitalWrite(rs,1);
 send4(x>>4);
 send4(x);
}

void printnum(int n){
 char a[5];
 sprintf(a,"%d",n);
 int i=0;
 while(a[i]!=0){
  data(a[i]);
  i++;
 }
}

void setup(){

 pinMode(rs,INPUT);
 pinMode(en,INPUT);
 pinMode(d4,INPUT);
 pinMode(d5,INPUT);
 pinMode(d6,INPUT);
 pinMode(d7,INPUT);

 delay(50);

 send4(3); delay(5);
 send4(3); delay(5);
 send4(3); delay(1);
 send4(2);

 cmd(0x28);
 cmd(0x0C);
 cmd(0x06);
 cmd(0x01);

 delay(5);

 data('H'); data('e'); data('l'); data('l'); data('o');
 data(' '); data('w'); data('o'); data('r'); data('l'); data('d');
}

void loop(){

 cmd(0xC0);

 printnum(count);

 delay(50); 

 count++;

 if(count>maxcount)   
 {
   count=1;
 }

 cmd(0x01);
 delay(5);

 data('H'); data('e'); data('l'); data('l'); data('o');
 data(' '); data('w'); data('o'); data('r'); data('l'); data('d');
}