/* android/stub/log/log.h: fdk-aac, compilado para Android, incluye esta
   cabecera interna de AOSP, que el NDK no trae, para avisar de un flujo de
   audio malformado. Aquí no hace nada. */
#pragma once
#define android_errorWriteLog(tag, subtag) ((void)(tag), (void)(subtag))
