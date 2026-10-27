/* Copyright (c) 2026 Tunuino. Templates, persisted profiles and exact units. */
package io.tunuino;
import io.tunuino.calibration.*;
import io.tunuino.protocol.MapWriteClient;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.util.Arrays;
public final class BaseConfigurationChecks {
    private static void check(boolean value,String reason){if(!value)throw new AssertionError(reason);}
    public static void main(String[] args)throws Exception{
        for(var template:BaseMaps.Template.values())for(var kind:CalibrationMap.Kind.values()){
            var map=BaseMaps.create(template,kind);byte[] wire=MapWriteClient.encode(map);
            check(map.rows()==16&&map.columns()==16&&wire.length==576,"All base maps fit firmware pages");
            check(map.rpmAt(0)==200&&map.rpmAt(15)==7000&&map.loadKpaAt(15)==105,"NA base axes include cranking without boosted cells");
            check(map.lookup(3000,150).clamped(),"Base does not pretend to calibrate boost");
            for(int r=0;r<16;r++)for(int c=0;c<16;c++)check(map.cell(r,c)>0,"No unpopulated base cells");
            if(template==BaseMaps.Template.FLAT_REFERENCE)check(map.lookup(3123,66).value()==(kind==CalibrationMap.Kind.VE?80:10),"Flat reference exact interpolation");
            check(Arrays.equals(map.encode(),CalibrationMap.decode(map.encode()).encode()),"Template files round trip");
        }
        int fuel=BaseMaps.gasolineRequiredFuelUs(2000,4,250);
        check(fuel>12900&&fuel<13000,"Nominal gasoline mass/flow calculation");
        check(BaseMaps.gasolineRequiredFuelUs(2000,4,500)==Math.round(fuel/2.0),"Twice the flow halves nominal pulse");
        byte[] p=new byte[128];p[0]=p[1]=1;ByteBuffer b=ByteBuffer.wrap(p);
        b.putShort(2,(short)3000).putShort(4,(short)2500).putShort(6,(short)500).putShort(8,(short)15000)
            .putShort(12,(short)9500).putShort(14,(short)9000);p[25]=1;p[26]=2;
        check(EngineProfile.valid(p,36,1),"Valid profile accepted");
        var stored=IoConfiguration.localDefaults().withEngine(p,false);
        check(stored.errors().isEmpty()&&!stored.autonomous(),"Manual profile may be stored before sensor calibration");
        var file=Files.createTempDirectory("tunuino-profile-backup-").resolve("motor.tncfg");
        try{stored.writeNew(file);check(Arrays.equals(p,IoConfiguration.read(file).engineProfile()),"Motor survives backup and restore");}
        finally{Files.deleteIfExists(file);Files.deleteIfExists(file.getParent());}
        byte[] original=stored.bytes();original[132]=0;
        check(stored.engineProfile()[4]!=0,"Profile storage is immutable");
        p[33]=12;check(!EngineProfile.valid(p,36,1),"Invalid ignition channel rejected");p[33]=0;
        p[22]=1;check(!EngineProfile.valid(p,36,1),"Relay/injector overlap rejected");p[22]=0;
        try{stored.withEngine(p,true);throw new AssertionError("Autonomous mode accepted missing sensors");}catch(IllegalArgumentException expected){}
        System.out.println("Base maps/profile: 16x16 upload encoding, explicit NA limits, reference values, nominal fuel calculation, backup roundtrip, wiring validation and autonomous prerequisites passed.");
    }
}
