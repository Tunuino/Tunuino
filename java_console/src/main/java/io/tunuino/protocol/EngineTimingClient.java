/* Copyright (c) 2026 Tunuino. Read-only dry-run timing decoder. */
package io.tunuino.protocol;

import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.time.Duration;

/** Numeric calculations for an uncalibrated reference: NOT fuel or ignition commands. */
public final class EngineTimingClient {
    private static final byte[] REQUEST="TUNUINO:T".getBytes(StandardCharsets.US_ASCII);
    private EngineTimingClient() {}

    public record Timing(boolean valid,int reasons,double rpm,long revolutionUs,long cycleUs,
                         long wheelSlotUs,double relativeLoadPercent,long sparkReferenceUs,
                         long generation) {
        public String explanation() {
            return valid ? "Tempos apenas teóricos; não são comandos de atuação"
                : EnginePreviewClient.explainReasons(reasons);
        }
    }
    public static Timing decode(byte[] body) throws IOException {
        if(body==null||body.length!=32||body[0]!=0||body[1]!=1
                ||(body[2]!=0&&body[2]!=1))
            throw new IOException("Resposta de temporização Tunuino inválida");
        ByteBuffer buffer=ByteBuffer.wrap(body);
        boolean valid=body[2]==1;
        int reasons=body[3]&255;
        long rpmMilli=Integer.toUnsignedLong(buffer.getInt(4));
        long revolution=Integer.toUnsignedLong(buffer.getInt(8));
        long cycle=Integer.toUnsignedLong(buffer.getInt(12));
        long tooth=Integer.toUnsignedLong(buffer.getInt(16));
        long load=Integer.toUnsignedLong(buffer.getInt(20));
        int spark=buffer.getInt(24);
        long generation=Integer.toUnsignedLong(buffer.getInt(28));
        if(generation==0||rpmMilli>20_000_000
                ||(valid && (reasons!=0||rpmMilli<1000||revolution<3000||cycle!=2*revolution
                    ||tooth==0||tooth>revolution||load==0||load>150000
                    ||Math.abs((long)spark)>revolution/4+1))
                ||(!valid && (reasons==0||revolution!=0||cycle!=0||tooth!=0
                    ||load!=0||spark!=0)))
            throw new IOException("Valores de temporização incoerentes");
        return new Timing(valid,reasons,rpmMilli/1000.0,revolution,cycle,tooth,
            load/100.0,spark,generation);
    }

    public static Timing read(PacketSession session,DeviceIdentity identity,Duration timeout)
            throws IOException {
        if(!EnginePreviewClient.supports(identity))
            throw new IllegalStateException("A ECU não suporta este perfil de cálculo");
        try {
            byte[] body=session.exchange(REQUEST,timeout);
            /* Older 0.1.0 firmware does not implement T. This is a normal
             * unsupported-command response; keep the functioning USB session. */
            if(body!=null&&body.length==1&&body[0]==(byte)0x83)
                throw new UnsupportedOperationException(
                    "Atualiza o firmware Tunuino para utilizar o cálculo temporal de bancada");
            return decode(body);
        } catch(IOException error) {
            try {session.close();}
            catch(IOException closing){error.addSuppressed(closing);}
            throw error;
        }
    }
}
