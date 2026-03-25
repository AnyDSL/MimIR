from enum import IntEnum

class core(IntEnum):
    ID = 0x1226322701853917184
    idx = 0x1104c60000000300
    idx_unsafe = 0x1104c60000000400
    minus = 0x1104c60000000900
    abs = 0x1104c60000000d00
    bitcast = 0x1104c60000000f00
    select = 0x1104c60000001200
    
    class nat(IntEnum):
        add = 0x1104c60000000000,
        sub = 0x1104c60000000001,
        mul = 0x1104c60000000002,
    

    class ncmp(IntEnum):
        glef = 0x1104c60000000100,
        glEe = 0x1104c60000000101,
        gLel = 0x1104c60000000102,
        gLEle = 0x1104c60000000103,
        Gleg = 0x1104c60000000104,
        GlEge = 0x1104c60000000105,
        GLene = 0x1104c60000000106,
        GLEt = 0x1104c60000000107,
    

    class mode(IntEnum):
        us = 0x1104c60000000200,
        uS = 0x1104c60000000201,
        Us = 0x1104c60000000202,
        US = 0x1104c60000000203,
        nuw = 0x1104c60000000204,
        nsw = 0x1104c60000000205,
        nusw = 0x1104c60000000206,
    

    class bit1(IntEnum):
        f = 0x1104c60000000500,
        neg = 0x1104c60000000501,
        id = 0x1104c60000000502,
        t = 0x1104c60000000503,
    

    class bit2(IntEnum):
        f = 0x1104c60000000600,
        nor = 0x1104c60000000601,
        nciff = 0x1104c60000000602,
        nfst = 0x1104c60000000603,
        niff = 0x1104c60000000604,
        nsnd = 0x1104c60000000605,
        xor_ = 0x1104c60000000606,
        nand = 0x1104c60000000607,
        and_ = 0x1104c60000000608,
        nxor = 0x1104c60000000609,
        snd = 0x1104c6000000060a,
        iff = 0x1104c6000000060b,
        fst = 0x1104c6000000060c,
        ciff = 0x1104c6000000060d,
        or_ = 0x1104c6000000060e,
        t = 0x1104c6000000060f,
    

    class shr(IntEnum):
        a = 0x1104c60000000700,
        l = 0x1104c60000000701,
    

    class wrap(IntEnum):
        add = 0x1104c60000000800,
        sub = 0x1104c60000000801,
        mul = 0x1104c60000000802,
        shl = 0x1104c60000000803,
    

    class div(IntEnum):
        sdiv = 0x1104c60000000a00,
        udiv = 0x1104c60000000a01,
        srem = 0x1104c60000000a02,
        urem = 0x1104c60000000a03,
    

    class icmp(IntEnum):
        xyglef = 0x1104c60000000b00,
        xyglEe = 0x1104c60000000b01,
        xygLe = 0x1104c60000000b02,
        xygLE = 0x1104c60000000b03,
        xyGle = 0x1104c60000000b04,
        xyGlE = 0x1104c60000000b05,
        xyGLe = 0x1104c60000000b06,
        xyGLE = 0x1104c60000000b07,
        xYgle = 0x1104c60000000b08,
        xYglE = 0x1104c60000000b09,
        xYgLesl = 0x1104c60000000b0a,
        xYgLEsle = 0x1104c60000000b0b,
        xYGleug = 0x1104c60000000b0c,
        xYGlEuge = 0x1104c60000000b0d,
        xYGLe = 0x1104c60000000b0e,
        xYGLE = 0x1104c60000000b0f,
        Xygle = 0x1104c60000000b10,
        XyglE = 0x1104c60000000b11,
        XygLeul = 0x1104c60000000b12,
        XygLEule = 0x1104c60000000b13,
        XyGlesg = 0x1104c60000000b14,
        XyGlEsge = 0x1104c60000000b15,
        XyGLe = 0x1104c60000000b16,
        XyGLE = 0x1104c60000000b17,
        XYgle = 0x1104c60000000b18,
        XYglE = 0x1104c60000000b19,
        XYgLe = 0x1104c60000000b1a,
        XYgLE = 0x1104c60000000b1b,
        XYGle = 0x1104c60000000b1c,
        XYGlE = 0x1104c60000000b1d,
        XYGLene = 0x1104c60000000b1e,
        XYGLEt = 0x1104c60000000b1f,
    

    class extrema(IntEnum):
        smumin = 0x1104c60000000c00,
        sMumax = 0x1104c60000000c01,
        Smsmin = 0x1104c60000000c02,
        SMsmax = 0x1104c60000000c03,
    

    class conv(IntEnum):
        s = 0x1104c60000000e00,
        u = 0x1104c60000000e01,
    

    class trait(IntEnum):
        size = 0x1104c60000001000,
        align = 0x1104c60000001001,
    

    class pe(IntEnum):
        hlt = 0x1104c60000001100,
        run = 0x1104c60000001101,
        is_closed = 0x1104c60000001102,
    

