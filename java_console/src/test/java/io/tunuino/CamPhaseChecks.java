/* Copyright (c) 2026 Tunuino. Protocol fixtures only, no physical validation. */
package io.tunuino;
import io.tunuino.protocol.CamPhaseClient;
import java.io.IOException;
import java.nio.ByteBuffer;

public final class CamPhaseChecks {
    private static void check(boolean value){if(!value)throw new AssertionError();}
    private static void rejected(byte[] b)throws Exception{
        try{CamPhaseClient.decode(b);throw new AssertionError("Accepted incoherent cam phase");}
        catch(IOException expected){}
    }
    public static void main(String[] args)throws Exception{
        byte[] b=new byte[32];ByteBuffer p=ByteBuffer.wrap(b);
        b[1]=1;b[2]=7;p.putInt(4,42125).putLong(8,4294968000L).putLong(16,4294969000L)
            .putInt(24,2).putShort(28,(short)60);b[30]=2;
        var phase=CamPhaseClient.decode(b);
        check(phase.valid()&&phase.crankDegrees()==421.25&&phase.capturedUs()==4294968000L
            &&phase.sampledUs()==4294969000L&&phase.description().contains("421.25"));
        for(int flags:new int[]{2,4,5,6,8,255}){b[2]=(byte)flags;rejected(b);}b[2]=7;
        p.putInt(4,72000);rejected(b);p.putInt(4,42125);
        p.putLong(8,4294969001L);rejected(b);p.putLong(8,4290969000L);rejected(b);
        p.putLong(8,4294968000L);p.putInt(24,0);rejected(b);p.putInt(24,2);
        b[31]=1;rejected(b);b[31]=0;b[2]=3;rejected(b);
        p.putInt(4,0).putLong(8,0);check(!CamPhaseClient.decode(b).valid());
        b[2]=0;check(CamPhaseClient.decode(b).description().contains("sem sincronismo"));
        rejected(null);rejected(new byte[31]);
        System.out.println("Cam phase: U64 timestamps, phase/parity, loss states, age and malformed responses passed (synthetic fixtures).");
    }
}
