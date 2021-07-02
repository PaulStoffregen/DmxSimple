/*
 * This code was created by dominpiano.
 * You can modify it as you want.
 * ONE IMPORTANT THING
 * This was created for LedBar from Light4Me, using channel C, (basically you have to have red on channel 1, green on channel 2,
 * and blue on channel 3. Optionally you can have master on channel 4, then look down to DmxSimple.write() functions.
 * 
 * 
 */

#include <DmxSimple.h>

float counter = 0;
int red, green, blue;

float master = 100; //change this from 0 to 100 to get different brightness

void setup() {
  
}

void loop() {
  setLedColorHSV(counter, 1, 1); //every single color from a whole circle (full saturation)
  if(counterColor >= 359){
    counter = 0;
  }else{
    counter++;
  }

  DmxSimple.write(1, red * (master/100));
  DmxSimple.write(2, green * (master/100));
  DmxSimple.write(3, blue * (master/100));
  /*
   * optionally if u have channel 4 for master go for this:
   * DmxSimple.write(1, red);
     DmxSimple.write(2, green);
     DmxSimple.write(3, blue);
     DmxSimple.write(4, master);

     
     of course you have to then set master on top to be between 0 and 255
   */
  delay(30); //change this to change speed of the animation

}

void setLedColorHSV(int h, double s, double v) {
  //this is the algorithm to convert from RGB to HSV
  double r=0; 
  double g=0; 
  double b=0;

  double hf=h/60.0;

  int i=(int)floor(h/60.0);
  double f = h/60.0 - i;
  double pv = v * (1 - s);
  double qv = v * (1 - s*f);
  double tv = v * (1 - s * (1 - f));

  switch (i)
  {
  case 0: //rojo dominante
    r = v;
    g = tv;
    b = pv;
    break;
  case 1: //verde
    r = qv;
    g = v;
    b = pv;
    break;
  case 2: 
    r = pv;
    g = v;
    b = tv;
    break;
  case 3: //azul
    r = pv;
    g = qv;
    b = v;
    break;
  case 4:
    r = tv;
    g = pv;
    b = v;
    break;
  case 5: //rojo
    r = v;
    g = pv;
    b = qv;
    break;
  }

  //set each component to a integer value between 0 and 255
  red = constrain((int)255*r,0,255);
  green = constrain((int)255*g,0,255);
  blue = constrain((int)255*b,0,255);


  
}
