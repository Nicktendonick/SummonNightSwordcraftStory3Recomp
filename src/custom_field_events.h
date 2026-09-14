#pragma once
namespace swordcraft3 {
// Candidate only: caller MUST also authenticate chief map + exact script bytes.
// These two observed states execute an audio command and a flag assignment,
// not dialogue. Do not generalize this to all short events or these opcodes
// at other addresses; real cutscenes also use sound and flag instructions.
constexpr bool chief_ambient_event_state(unsigned flags,unsigned script,
    unsigned base,unsigned active,unsigned state,unsigned opcode,unsigned ip) {
    return flags==4 && script==201 && base==0x02006000 && active==1 &&
        ((state==5 && opcode==0x033c && ip==0x02006cde) ||
         (state==2 && opcode==0x0001 && ip==0x02006cee));
}
}
