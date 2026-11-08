// @0x38bc4  Tama2MovieMain_Implement::initCharacterData(int, int)
void Tama2MovieMain_Implement::initCharacterData(int arg1, int arg2) {
    uint32_t uVar1;
    uint32_t uVar2;
    uint32_t uVar3;
    uint32_t *puVar4;
    uint32_t *puVar5;
    int iVar6;
    int iVar7;

    iVar7 = &DAT_bc6fc;
    // @0xc1ba8  Tama2MovieMain_Implement::m_para
    puVar5 = &m_para;
    if (arg1 == 0) {
        puVar5[2] = 1;
        puVar5[4] = 0x40;
        *puVar5 = 0;
        puVar5[1] = 0;
        puVar5[5] = 0;
        puVar5[6] = 0;
        puVar5[7] = 0;
        puVar5[8] = 0;
        puVar5[9] = 0;
        // @0x33628  Tama2MovieMain_Implement::setParaTimer(int)
        setParaTimer(10);
        setParaTimer(0xb);
        iVar6 = 0x4e0;
        uVar2 = *puVar5;
        puVar5[0xc] = -1;
        puVar5[0xd] = -1;
        // @0xa3890  Tama2MovieMain_Implement::DATA_PARAMETER
        iVar6 = &DATA_PARAMETER + uVar2 * 0x24;
        uVar1 = iVar6->field_0x4;
        uVar3 = iVar6->field_0x20;
        puVar5[0x10] = -1;
        puVar5[0x11] = -1;
        puVar5[0xf] = uVar3;
        puVar5[0x20] = 0;
        puVar5[0x21] = 0;
        puVar5[0x22] = 0;
        puVar5[0x14] = -1;
        puVar5[0x15] = -1;
        puVar5[0x16] = -1;
        puVar5[0x17] = -1;
        puVar5[0x23] = -1;
        puVar5[0xe] = uVar1;
        puVar5[0x12] = 0;
        puVar5[0x13] = 0;
        puVar5[0x18] = 0;
        puVar5[0x19] = 0;
        puVar5[0x1f] = 0;
        puVar5[0x1e] = 0;
        puVar5[0x24] = 0;
        goto L_38c6e;
    }
    if (*puVar5 == arg2) {
        return;
    }
    *puVar5 = arg2;
    // @0x38b70  Tama2MovieMain_Implement::doCountupGochCount()
    doCountupGochCount();
    uVar3 = *puVar5;
    iVar6 = &DATA_PARAMETER;
    uVar1 = *(iVar6 + uVar3 * 0x24);
    if (puVar5[2] < uVar1) {
        puVar5[2] = uVar1;
    }
    uVar1 = *(iVar6 + uVar3 * 0x24 + 0x1c);
    puVar5[4] = 0xfff0fff7 & puVar5[4];
    puVar5[9] = uVar1;
    if (uVar1 == 2) {
        // @0xc01c4  m_rand
        m_rand = 0x41c64e6d * m_rand + 0x3039 & 0x7fff;
        puVar5[9] = (m_rand * 100 <= 0x197fff) + 2;
    }
    if (uVar3 == 1) {
        puVar5[0xc] = 0xf;
        puVar5[0xd] = 0x2d;
        puVar5[10] = 0;
        puVar5[0xb] = 2;
        puVar5[0xe] = 0x3c;
        // @0xc1ba0  Tama2MovieMain_Implement::m_firstFlg
        m_firstFlg = 1;
        uVar2 = 1;
        uVar1 = iVar6->field_0x44;
        if (uVar1 < 1) goto L_38d32;
L_38c40:
        setParaTimer(0xf);
        uVar2 = *puVar5;
    }
    else {
        puVar5[0xc] = 0xb4;
        setParaTimer(0xd);
        setParaTimer(10);
        setParaTimer(0xb);
        uVar2 = *puVar5;
        uVar1 = *(iVar6 + uVar2 * 0x24 + 0x20);
        puVar5[0x20] = *(iVar6 + uVar2 * 0x24 + 4);
        puVar5[0xe] = -1;
        if (uVar1 > 0) goto L_38c40;
L_38d32:
        puVar5[0xf] = uVar1;
    }
    puVar5[0x21] = 0;
    puVar5[0x12] = 0;
    puVar5[0x13] = 0;
    puVar5[0x18] = 0;
    puVar5[0x19] = 0;
    puVar5[0x1f] = 0;
    puVar5[0x1e] = 0;
    puVar5[0x22] = 0;
    puVar5[0x14] = -1;
    puVar5[0x15] = -1;
    puVar5[0x16] = -1;
    puVar5[0x17] = -1;
    puVar5[0x23] = -1;
L_38c6e:
    // @0xc1d50  Tama2MovieMain_Implement::m_recData
    iVar7 = &m_recData;
    puVar4 = iVar7 + 0x18;
    while( true ) {
        puVar5 += 1;
        *puVar4 = uVar2;
        puVar4 += 1;
        if (puVar4 == iVar7 + 0xd8) break;
        uVar2 = *puVar5;
    }
    // @0x32e90  Tama2MovieMain_Implement::saveRecord()
    saveRecord();
}
