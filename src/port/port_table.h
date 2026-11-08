#define JAVA(method) "com/namcobandaigames/tamagotchisp/SampleGameNatives." method

NATIVE(JAVA("OnInputStreamOpenLocalFile(Ljava/lang/String;)I"), native_OnInputStreamOpenLocalFile)
NATIVE(JAVA("OnOutputStreamOpenLocalFile(Ljava/lang/String;)I"), native_OnOutputStreamOpenLocalFile)
NATIVE(JAVA("OnOutputStreamClose(I)I"), native_OnOutputStreamClose)
NATIVE(JAVA("OnOutputStreamWrite(I[B)I"), native_OnOutputStreamWrite)
NATIVE("Device_PrintMessage", native_Device_PrintMessage)
