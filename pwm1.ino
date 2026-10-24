void setup()
{
    pinMode(9, OUTPUT);   
}

void loop()
{
    for (int i = 0; i <= 255; i++)
    {
        analogWrite(9, i);  
        delay(20);
    }

    for (int i = 255; i >= 0; i--)
    {
        analogWrite(9, i);
        delay(20);
    }
}