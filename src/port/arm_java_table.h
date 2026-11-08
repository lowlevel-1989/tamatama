/* port/arm_java_table.h: los métodos de Java que atiende el propio loader-arm,
   nunca una función nativa, porque en el PC las nativas corren en otro proceso.
   Una línea por método:
   ARM_JAVA("Clase.nombre(firma)", función de loader-arm). Lo incluye
   android_emu/jvm.c. */
ARM_JAVA("com/namcobandaigames/tamagotchisp/SampleGameNatives.OnThreadCreate(II)I", java_thread_create)
