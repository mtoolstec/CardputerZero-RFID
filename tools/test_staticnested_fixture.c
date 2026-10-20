// Synthetic Crypto1 vector generator; this is not a captured card session.
#include <stdio.h>
#include <inttypes.h>
#include "crapto1/crapto1.h"
int main(void) {
    const uint32_t uid=0x11223344, nt=0x01200145;
    const uint64_t key=UINT64_C(0xFFFFFFFFFF09);
    uint32_t encrypted[2];
    for (int i=0; i<2; ++i) {
        const uint32_t nonce=prng_successor(nt,160*(i+1));
        struct Crypto1State state;
        crypto1_init(&state,key);
        encrypted[i]=nonce ^ crypto1_word(&state,nonce ^ uid,0);
    }
    printf("0x%08"PRIx32" 0x60 0x%08"PRIx32" 0x%08"PRIx32" 0x%08"PRIx32" 0x%08"PRIx32"\n",uid,nt,encrypted[0],nt,encrypted[1]);
    return 0;
}
