// Esto se llama desde initCharacterData y doControlIvent, después de copiar
// m_para en m_recData, y desde saveSmartpSystemStatus.
// @0x32e90  Tama2MovieMain_Implement::saveRecord()
//
// Guarda la partida entera en gamestatus_0.dat. En disco se llama
// Tama2Movie_gamestatus_0.dat: el prefijo lo pone el camino de
// OnOutputStreamOpenLocalFile. Son tres bloques de 0x100 bytes seguidos,
// 768 bytes en total:
//
//   offset  bytes  qué
//   0x000   0x100  m_smartpSystemStatus, tal cual
//   0x100   0x100  m_recData: 64 int, cada uno en big endian
//   0x200   0x100  m_smartpExtendRecord, tal cual
//
// m_recData va con writeInt, que es de Java: escribe cada int en big endian,
// al revés que el ARM. Para leerlo hay que darle la vuelta a cada grupo de
// 4 bytes.
//
// initCharacterData y doControlIvent copian m_para[0x0] a m_para[0x2f] en
// m_recData[6] a m_recData[53], así que en el fichero m_para[i] está en
//   0x100 + (6 + i) * 4 = 0x118 + i * 4
// Por ejemplo, ID_00 en 0x118 y TRANSFORM_0e en 0x150 (doc/code/m_para.h).
// m_recData[0] a m_recData[5] y m_recData[54] a m_recData[63] son otra cosa,
// sin descubrir aún.
void Tama2MovieMain_Implement::saveRecord(void) {
    // @0x30a2c  String::String(char const*)
    String name("gamestatus_0.dat");
    // @0x310a4  Connector::openDataOutputStream(Tama2MovieJava::String const&)
    DataOutputStream out = Connector::openDataOutputStream(name);
    name.~String();

    // @0x2fe70  DataOutputStream::write(char const*, int)
    // @0xc00c0  Tama2MovieMain_Implement::m_smartpSystemStatus
    out.write(&m_smartpSystemStatus, 0x100);

    // @0xc1d50  Tama2MovieMain_Implement::m_recData
    for (int i = 0; i < 64; i++) {
        // @0x2fe88  DataOutputStream::writeInt(int)
        out.writeInt(m_recData[i]);   // big endian
    }

    // @0xbffb8  Tama2MovieMain_Implement::m_smartpExtendRecord
    out.write(&m_smartpExtendRecord, 0x100);

    // @0x308c0  OutputStream::close()
    out.close();
    // @0x30100  System::gc()
    System::gc();
    out.~DataOutputStream();
}
