/* Copyright (c) 2026 Tunuino. Actual C board adapter, synthetic registers. */
package io.tunuino;
import io.tunuino.protocol.EngineOutputClient;
import io.tunuino.transport.PacketSession;
import io.tunuino.transport.ProcessSerialChannel;
import java.nio.ByteBuffer;
import java.util.List;
public final class EngineOutputChecks {
    private static void check(boolean value,String reason){if(!value)throw new AssertionError(reason);}
    public static void main(String[] args)throws Exception {
        try(var session=new PacketSession(new ProcessSerialChannel(List.of(args[0])))){
            var off=EngineOutputClient.read(session);check(!off.armed()&&!off.configured()&&off.timerReady(),"Actual board starts disarmed");
            var pulse=EngineOutputClient.pulse(session,16,2000);check(pulse.mode()==4&&pulse.pending()==(1L<<16),"Java pulse command reaches the actual GPIO scheduler");
            check(EngineOutputClient.stop(session).mode()==0,"Explicit stop cancels pending hardware pulse");
            byte[] p=new byte[128];ByteBuffer b=ByteBuffer.wrap(p);p[0]=p[1]=1;
            b.putShort(2,(short)3000).putShort(4,(short)2000).putShort(6,(short)500).putShort(8,(short)15000)
                .putShort(12,(short)9500).putShort(14,(short)9000).putInt(34,18000);p[22]=16;p[25]=1;p[26]=2;
            var configured=EngineOutputClient.configure(session,p);check(configured.configured()&&configured.cylinders()==1&&!configured.armed(),"Profile configures without energizing anything");
            var aux=EngineOutputClient.auxiliaryTest(session,15,500);
            check(aux.mode()==4&&aux.pending()==(1L<<15),"Assigned relay test reaches actual board scheduler");
            EngineOutputClient.stop(session);
            try{EngineOutputClient.auxiliaryTest(session,0,500);throw new AssertionError("Injector accepted a relay pulse");}
            catch(java.io.IOException expected){}
            try{EngineOutputClient.auxiliaryTest(session,16,100);throw new AssertionError("IGN accepted relay timing");}
            catch(IllegalArgumentException expected){}
            try{EngineOutputClient.auxiliaryTest(session,15,801);throw new AssertionError("Relay duration cap bypassed");}
            catch(IllegalArgumentException expected){}
            try{EngineOutputClient.arm(session,configured.generation()+1);throw new AssertionError("Stale generation armed");}
            catch(java.io.IOException expected){check(session.isOpen(),"Refusal preserves serial session");}
            var armed=EngineOutputClient.arm(session,configured.generation());check(armed.mode()==1&&armed.active()==0,"No crank/cam sync means no injector or coil energization");
            try{EngineOutputClient.pulse(session,0,1000);throw new AssertionError("Bench pulse bypassed engine arming");}catch(java.io.IOException expected){}
            try{EngineOutputClient.auxiliaryTest(session,15,100);throw new AssertionError("Relay test bypassed engine arming");}catch(java.io.IOException expected){}
            check(EngineOutputClient.stop(session).active()==0,"Stop acknowledgement reflects actual inactive GPIO");
            p[33]=12;try{EngineOutputClient.configure(session,p);throw new AssertionError("Invalid coil accepted");}catch(java.io.IOException expected){}
            try{EngineOutputClient.pulse(session,16,5001);throw new AssertionError("Dwell cap bypassed");}catch(IllegalArgumentException expected){}
            p[33]=0;
            var identity=new io.tunuino.protocol.DeviceIdentity(io.tunuino.protocol.DeviceIdentity.Family.PROTEUS,
                io.tunuino.protocol.DeviceIdentity.McuSeries.STM32_F4,7,0,2,0,15,"010000000000000000000000");
            var timeout=java.time.Duration.ofSeconds(10);
            var before=io.tunuino.protocol.ConfigurationClient.read(session,identity,timeout);
            var saved=io.tunuino.protocol.ConfigurationClient.apply(session,identity,before,before.configuration().withEngine(p,false),timeout);
            check(java.util.Arrays.equals(p,saved.configuration().engineProfile()),"Persisted motor page reads back exactly");
            var restored=EngineOutputClient.read(session);
            check(restored.persisted()&&restored.configured()&&!restored.autonomous()&&!restored.armed(),"Actual board loads saved profile without arming on save");
            check(io.tunuino.protocol.IoClient.read(session,identity,timeout).generation()==saved.validation().generation(),"Runtime with persisted-engine flags decodes in the Console");
            EngineOutputClient.arm(session,restored.generation());
            byte[] write={'w',0,1,0,96,0,2,0x17,0x70};
            check(session.exchange(write,timeout)[0]==(byte)0x84,"Running configuration writes are rejected before staging");
            check(!io.tunuino.protocol.ConfigurationClient.validation(session,identity,timeout).dirty(),"Rejected edit leaves active engine configuration unchanged");
            EngineOutputClient.stop(session);
        }
        System.out.println("Engine outputs C/Java: actual board protocol, profile, generation gate, disarmed bench pulse, no-sync inhibition, explicit stop and channel/dwell bounds passed. Synthetic registers only.");
    }
}
