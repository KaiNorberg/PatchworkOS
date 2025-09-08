#include "aml_op.h"

#include <stdlib.h>

// Normal ops (0x00–0xFF)
static const aml_op_props_t opsNormal[0x100] = {
    [0x00] = {"ZeroOp", AML_ENCODING_GROUP_DATA, AML_OP_FLAG_NONE},
    [0x01] = {"OneOp", AML_ENCODING_GROUP_DATA, AML_OP_FLAG_NONE},
    [0x06] = {"AliasOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NAMESPACE_MODIFIER},
    [0x08] = {"NameOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NAMESPACE_MODIFIER},
    [0x0A] = {"BytePrefix", AML_ENCODING_GROUP_DATA, AML_OP_FLAG_NONE},
    [0x0B] = {"WordPrefix", AML_ENCODING_GROUP_DATA, AML_OP_FLAG_NONE},
    [0x0C] = {"DWordPrefix", AML_ENCODING_GROUP_DATA, AML_OP_FLAG_NONE},
    [0x0D] = {"StringPrefix", AML_ENCODING_GROUP_DATA, AML_OP_FLAG_NONE},
    [0x0E] = {"QWordPrefix", AML_ENCODING_GROUP_DATA, AML_OP_FLAG_NONE},
    [0x10] = {"ScopeOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NAMESPACE_MODIFIER},
    [0x11] = {"BufferOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x12] = {"PackageOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x13] = {"VarPackageOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x14] = {"MethodOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x15] = {"ExternalOp", AML_ENCODING_GROUP_NAME, AML_OP_FLAG_NONE},
    [0x2E] = {"DualNamePrefix", AML_ENCODING_GROUP_NAME, AML_OP_FLAG_NONE},
    [0x2F] = {"MultiNamePrefix", AML_ENCODING_GROUP_NAME, AML_OP_FLAG_NONE},
    [0x30 ... 0x39] = {"DigitChar", AML_ENCODING_GROUP_NAME, AML_OP_FLAG_NONE},
    [0x41 ... 0x5A] = {"NameChar", AML_ENCODING_GROUP_NAME, AML_OP_FLAG_NONE},
    [0x5B] = {"ExtOpPrefix", AML_ENCODING_GROUP_NONE, AML_OP_FLAG_NONE},
    [0x5C] = {"RootChar", AML_ENCODING_GROUP_NAME, AML_OP_FLAG_NONE},
    [0x5E] = {"ParentPrefixChar", AML_ENCODING_GROUP_NAME, AML_OP_FLAG_NONE},
    [0x5F] = {"NameChar", AML_ENCODING_GROUP_NAME, AML_OP_FLAG_NONE},
    [0x60] = {"Local0Op", AML_ENCODING_GROUP_LOCAL, AML_OP_FLAG_NONE},
    [0x61] = {"Local1Op", AML_ENCODING_GROUP_LOCAL, AML_OP_FLAG_NONE},
    [0x62] = {"Local2Op", AML_ENCODING_GROUP_LOCAL, AML_OP_FLAG_NONE},
    [0x63] = {"Local3Op", AML_ENCODING_GROUP_LOCAL, AML_OP_FLAG_NONE},
    [0x64] = {"Local4Op", AML_ENCODING_GROUP_LOCAL, AML_OP_FLAG_NONE},
    [0x65] = {"Local5Op", AML_ENCODING_GROUP_LOCAL, AML_OP_FLAG_NONE},
    [0x66] = {"Local6Op", AML_ENCODING_GROUP_LOCAL, AML_OP_FLAG_NONE},
    [0x67] = {"Local7Op", AML_ENCODING_GROUP_LOCAL, AML_OP_FLAG_NONE},
    [0x68] = {"Arg0Op", AML_ENCODING_GROUP_ARG, AML_OP_FLAG_NONE},
    [0x69] = {"Arg1Op", AML_ENCODING_GROUP_ARG, AML_OP_FLAG_NONE},
    [0x6A] = {"Arg2Op", AML_ENCODING_GROUP_ARG, AML_OP_FLAG_NONE},
    [0x6B] = {"Arg3Op", AML_ENCODING_GROUP_ARG, AML_OP_FLAG_NONE},
    [0x6C] = {"Arg4Op", AML_ENCODING_GROUP_ARG, AML_OP_FLAG_NONE},
    [0x6D] = {"Arg5Op", AML_ENCODING_GROUP_ARG, AML_OP_FLAG_NONE},
    [0x6E] = {"Arg6Op", AML_ENCODING_GROUP_ARG, AML_OP_FLAG_NONE},
    [0x70] = {"StoreOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x71] = {"RefOfOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x72] = {"AddOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x73] = {"ConcatOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x74] = {"SubtractOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x75] = {"IncrementOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x76] = {"DecrementOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x77] = {"MultiplyOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x78] = {"DivideOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x79] = {"ShiftLeftOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x7A] = {"ShiftRightOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x7B] = {"AndOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x7C] = {"NandOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x7D] = {"OrOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x7E] = {"NorOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x7F] = {"XorOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x80] = {"NotOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x81] = {"FindSetLeftBitOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x82] = {"FindSetRightBitOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x83] = {"DerefOfOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x84] = {"ConcatResOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x85] = {"ModOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x86] = {"NotifyOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_STATEMENT},
    [0x87] = {"SizeOfOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x88] = {"IndexOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x89] = {"MatchOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x8A] = {"CreateDWordFieldOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x8B] = {"CreateWordFieldOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x8C] = {"CreateByteFieldOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x8D] = {"CreateBitFieldOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x8E] = {"ObjectTypeOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x8F] = {"CreateQWordFieldOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x90] = {"LandOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x91] = {"LorOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x92] = {"LnotOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x93] = {"LEqualOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x94] = {"LGreaterOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x95] = {"LLessOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x96] = {"ToBufferOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x97] = {"ToDecimalStringOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x98] = {"ToHexStringOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x99] = {"ToIntegerOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x9C] = {"ToStringOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x9D] = {"CopyObjectOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x9E] = {"MidOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x9F] = {"ContinueOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_STATEMENT},
    [0xA0] = {"IfOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_STATEMENT},
    [0xA1] = {"ElseOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_STATEMENT},
    [0xA2] = {"WhileOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_STATEMENT},
    [0xA3] = {"NoopOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_STATEMENT},
    [0xA4] = {"ReturnOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_STATEMENT},
    [0xA5] = {"BreakOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_STATEMENT},
    [0xCC] = {"BreakPointOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_STATEMENT},
    [0xFF] = {"OnesOp", AML_ENCODING_GROUP_DATA, AML_OP_FLAG_NONE},
};

// Extended ops prefixed with 0x5B
static const aml_op_props_t opsExt5b[0x100] = {
    [0x01] = {"MutexOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x02] = {"EventOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x12] = {"CondRefOfOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x13] = {"CreateFieldOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x1F] = {"LoadTableOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x20] = {"LoadOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x21] = {"StallOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_STATEMENT},
    [0x22] = {"SleepOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_STATEMENT},
    [0x23] = {"AcquireOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x24] = {"SignalOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_STATEMENT},
    [0x25] = {"WaitOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x26] = {"ResetOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_STATEMENT},
    [0x27] = {"ReleaseOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_STATEMENT},
    [0x28] = {"FromBCDOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x29] = {"ToBCD", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x2A] = {"Reserved", AML_ENCODING_GROUP_NONE, AML_OP_FLAG_NONE},
    [0x30] = {"RevisionOp", AML_ENCODING_GROUP_DATA, AML_OP_FLAG_NONE},
    [0x31] = {"DebugOp", AML_ENCODING_GROUP_DEBUG, AML_OP_FLAG_NONE},
    [0x32] = {"FatalOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_STATEMENT},
    [0x33] = {"TimerOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x80] = {"OpRegionOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x81] = {"FieldOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x82] = {"DeviceOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x84] = {"PowerResOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x85] = {"ThermalZoneOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x86] = {"IndexFieldOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x87] = {"BankFieldOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
    [0x88] = {"DataRegionOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_NONE},
};

// Extended ops prefixed with 0x92
static const aml_op_props_t opsExt92[0x100] = {
    [0x93] = {"LNotEqualOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x94] = {"LLessEqualOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
    [0x95] = {"LGreaterEqualOp", AML_ENCODING_GROUP_TERM, AML_OP_FLAG_EXPRESSION},
};

const aml_op_props_t* aml_op_lookup(uint8_t op, uint8_t extension)
{
    const aml_op_props_t* props = NULL;
    if (extension == 0)
    {
        props = &opsNormal[op];
    }
    else if (extension == 0x5B)
    {
        props = &opsExt5b[op];
    }
    else if (extension == 0x92)
    {
        props = &opsExt92[op];
    }
    else
    {
        return NULL;
    }

    if (props->name == NULL)
    {
        return NULL;
    }

    return props;
}

uint64_t aml_op_read(aml_state_t* state, aml_op_t* out, aml_op_flags_t flags)
{
    uint8_t bytes[2];
    uint64_t byteAmount = aml_bytes_peek(state, bytes, sizeof(bytes));

    if (byteAmount == 2)
    {
        if (bytes[0] == 0x92)
        {
            const aml_op_props_t* props = aml_op_lookup(bytes[1], 0x92);
            if (props == NULL)
            {
                return ERR;
            }

            if (!(flags & props->flags))
            {
                return ERR;
            }

            out->num = AML_OP_EXT92_BASE + bytes[1];
            out->length = 2;
            out->props = props;
            aml_advance(state, 2);
            return 0;
        }
        else if (bytes[0] == 0x5B)
        {
            const aml_op_props_t* props = aml_op_lookup(bytes[1], 0x5B);
            if (props == NULL)
            {
                return ERR;
            }

            if (!(flags & props->flags))
            {
                return ERR;
            }

            out->num = AML_OP_EXT5B_BASE + bytes[1];
            out->length = 2;
            out->props = props;
            aml_advance(state, 2);
            return 0;
        }
    }

    const aml_op_props_t* props = aml_op_lookup(bytes[0], 0);
    if (props == NULL)
    {
        return ERR;
    }

    if (!(flags & props->flags))
    {
        return ERR;
    }

    out->num = bytes[0];
    out->length = 1;
    out->props = props;
    aml_advance(state, 1);
    return 0;
}
