/* Copyright (c) 2026 Tunuino. Read-only captured cam phase, no VVT commands. */
package io.tunuino.protocol;

import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.Optional;

public final class CamPhaseClient {
    private CamPhaseClient() {}
    public record Phase(int flags,double crankDegrees,long capturedUs,long sampledUs,
                        long generation,int slots,int missing) {
        public boolean valid(){return (flags&4)!=0;}
        public String description(){
            return valid()?String.format(java.util.Locale.ROOT,"Cames: %.2f° cambota · idade %.1f ms",
                crankDegrees,(sampledUs-capturedUs)/1000.0):
                (flags&1)==0?"Cames: sem sincronismo de cambota":"Cames: aguarda confirmação de fase";
        }
    }
    public static Phase decode(byte[] b)throws IOException {
        if(b==null||b.length!=32||b[0]!=0||b[1]!=1||b[3]!=0||b[31]!=0)
            throw new IOException("Resposta da fase de cames inválida");
        ByteBuffer p=ByteBuffer.wrap(b);int flags=b[2]&255;
        long angle=Integer.toUnsignedLong(p.getInt(4)),captured=p.getLong(8),sampled=p.getLong(16);
        long generation=Integer.toUnsignedLong(p.getInt(24));int slots=Short.toUnsignedInt(p.getShort(28)),missing=b[30]&255;
        if((flags!=0&&flags!=1&&flags!=3&&flags!=7)||angle>=72000||captured<0||sampled<0
            ||captured>sampled||generation==0||slots<4||slots>120||missing<1||missing>=slots/2
            ||((flags&4)==0&&(angle!=0||captured!=0))||((flags&4)!=0&&sampled-captured>=4_000_000))
            throw new IOException("Valores da fase de cames incoerentes");
        return new Phase(flags,angle/100.0,captured,sampled,generation,slots,missing);
    }
    /** An older IO firmware may explicitly decline H without losing its USB session. */
    public static Optional<Phase> optional(PacketSession session)throws IOException {
        byte[] b=session.exchange("TUNUINO:H".getBytes(StandardCharsets.US_ASCII),Duration.ofMillis(500));
        if(b.length==1&&b[0]==(byte)0x83)return Optional.empty();
        return Optional.of(decode(b));
    }
}
