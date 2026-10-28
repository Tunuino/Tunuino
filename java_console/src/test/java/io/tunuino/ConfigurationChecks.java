/* Copyright (c) 2026 Tunuino. Actual C peer with synthetic inputs and modeled flash. */
package io.tunuino;

import io.tunuino.calibration.IoConfiguration;
import io.tunuino.protocol.*;
import io.tunuino.transport.*;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.Arrays;
import java.util.EnumMap;
import java.util.List;
import java.util.Map;

public final class ConfigurationChecks {
    private static void check(boolean condition,String message){if(!condition)throw new AssertionError(message);}
    private static final Duration TIMEOUT=Duration.ofSeconds(10);
    private static IoConfiguration configured(IoConfiguration original){
        var values=new EnumMap<IoConfiguration.Field,String>(IoConfiguration.Field.class);
        values.put(IoConfiguration.Field.MAP_RAW_LOW,"1000");values.put(IoConfiguration.Field.MAP_RAW_HIGH,"3000");
        values.put(IoConfiguration.Field.MAP_LOW,"0");values.put(IoConfiguration.Field.MAP_HIGH,"200");
        values.put(IoConfiguration.Field.LAMBDA_RAW_LOW,"1000");values.put(IoConfiguration.Field.LAMBDA_RAW_HIGH,"3000");
        values.put(IoConfiguration.Field.LAMBDA_LOW,"0.5");values.put(IoConfiguration.Field.LAMBDA_HIGH,"1.5");
        values.put(IoConfiguration.Field.TEMP_RAW_1,"1000");values.put(IoConfiguration.Field.TEMP_RAW_2,"2000");values.put(IoConfiguration.Field.TEMP_RAW_3,"3000");
        values.put(IoConfiguration.Field.TEMP_1,"100");values.put(IoConfiguration.Field.TEMP_2,"50");values.put(IoConfiguration.Field.TEMP_3,"-10.25");
        return original.edited(values,true,1,2,1).withFuelCorrections(io.tunuino.calibration.FuelCorrections.reference());
    }
    public static void main(String[] args)throws Exception{
        Path directory=Files.createTempDirectory("tunuino-configuration-");
        try(var session=new PacketSession(new ProcessSerialChannel(List.of(args[0])))){
            var identity=IdentityClient.identify(session,DeviceIdentity.Family.PROTEUS,Duration.ofSeconds(2));
            var first=ConfigurationClient.read(session,identity,TIMEOUT);
            check(!first.validation().dirty()&&!first.validation().persisted()&&first.validation().errors()==0,"Factory configuration diagnostics");
            var ve=MapReadClient.read(session,identity,io.tunuino.calibration.CalibrationMap.Kind.VE,TIMEOUT);
            var ign=MapReadClient.read(session,identity,io.tunuino.calibration.CalibrationMap.Kind.IGNITION,TIMEOUT);
            var candidate=configured(first.configuration());
            check(candidate.errors().isEmpty()&&candidate.value(IoConfiguration.Field.TEMP_3).equals("-10.25"),"Exact signed temperature");
            byte[] copy=candidate.bytes();copy[100]=0;check(!Arrays.equals(copy,candidate.bytes()),"Defensive configuration storage");
            Path backup=directory.resolve("settings.tncfg");candidate.writeNew(backup);
            check(Arrays.equals(candidate.bytes(),IoConfiguration.read(backup).bytes()),"Configuration backup/restore");
            try{candidate.writeNew(backup);throw new AssertionError("Existing backup overwritten");}catch(IOException expected){/* preserve */}
            byte[] corrupt=Files.readAllBytes(backup);corrupt[40]^=1;Files.write(backup,corrupt);
            try{IoConfiguration.read(backup);throw new AssertionError("Bad CRC accepted");}catch(IOException expected){/* corruption */}
            try{candidate.edited(Map.of(IoConfiguration.Field.MAP_HIGH,"200.001"),true,1,2,1);throw new AssertionError("Silently rounded calibration");}
            catch(IllegalArgumentException expected){check(session.isOpen(),"Local rejection preserves connection");}
            var saved=ConfigurationClient.apply(session,identity,first,candidate,TIMEOUT);
            check(saved.validation().persisted()&&!saved.validation().dirty()&&saved.validation().generation()==2
                &&Arrays.equals(saved.configuration().bytes(),candidate.bytes()),"Stage/validate/burn/readback");
            var live=IoClient.read(session,identity,TIMEOUT);
            check(live.sample().validMask()==15&&live.sample().mapKpa()==100&&live.sample().lambda()==1&&live.sample().coolantC()==50,"Applied settings drive actual C calibration");
            check(Arrays.equals(ve.encode(),MapReadClient.read(session,identity,ve.kind(),TIMEOUT).encode())
                &&Arrays.equals(ign.encode(),MapReadClient.read(session,identity,ign.kind(),TIMEOUT).encode()),"Settings preserve both complete maps");
            var same=ConfigurationClient.apply(session,identity,saved,candidate,TIMEOUT);check(same.validation().generation()==2,"Identical settings do not burn flash");
            try{ConfigurationClient.apply(session,identity,first,candidate,TIMEOUT);throw new AssertionError("Stale snapshot accepted");}catch(IOException expected){check(session.isOpen(),"Stale snapshot rejected before staging");}
            byte[] stage=ByteBuffer.allocate(9).put((byte)'w').putShort((short)1).putShort((short)100).putShort((short)2).putShort((short)3290).array();
            session.exchange(stage,TIMEOUT);var pending=ConfigurationClient.read(session,identity,TIMEOUT);
            check(pending.validation().dirty(),"Detect externally pending edits");
            try{ConfigurationClient.apply(session,identity,saved,candidate,TIMEOUT);throw new AssertionError("Pending edits overwritten");}catch(IOException expected){check(ConfigurationClient.read(session,identity,TIMEOUT).validation().dirty(),"Pending edits preserved");}
            var discarded=ConfigurationClient.discard(session,identity,TIMEOUT);
            check(!discarded.validation().dirty()&&Arrays.equals(candidate.bytes(),discarded.configuration().bytes()),"Explicit discard restores active configuration");
            byte[] invalidReport=new byte[16];invalidReport[1]=1;
            try{ConfigurationClient.decodeValidation(invalidReport);throw new AssertionError("Zero generation accepted");}catch(IOException expected){/* metadata */}
        }
        try(var session=new PacketSession(new ProcessSerialChannel(List.of(args[0],"--save-fails")))){
            var identity=IdentityClient.identify(session,DeviceIdentity.Family.PROTEUS,TIMEOUT);var original=ConfigurationClient.read(session,identity,TIMEOUT);
            try{ConfigurationClient.apply(session,identity,original,configured(original.configuration()),TIMEOUT);throw new AssertionError("Persistence failure reported as success");}
            catch(IOException expected){check(expected.getMessage().contains("nova leitura"),"Uncertain burn requires readback before retry");}
            var failed=ConfigurationClient.read(session,identity,TIMEOUT);
            check(failed.validation().dirty()&&!failed.validation().persisted()&&failed.validation().generation()==1,"Failed burn preserves staged data and active generation");
        }
        try(var session=new PacketSession(new ProcessSerialChannel(List.of(args[0],"--save-fails")))){
            var identity=IdentityClient.identify(session,DeviceIdentity.Family.PROTEUS,TIMEOUT);
            var map=MapReadClient.read(session,identity,io.tunuino.calibration.CalibrationMap.Kind.VE,TIMEOUT).withCell(0,0,"40");
            try{MapWriteClient.write(session,identity,map,TIMEOUT);throw new AssertionError("Map persistence failure reported as success");}
            catch(IOException expected){check(expected.getMessage().contains("confirmação da gravação na flash")&&expected.getMessage().contains("nova leitura"),"Map error identifies the failed phase and uncertain commit");}
            var failed=ConfigurationClient.read(session,identity,TIMEOUT);
            check(failed.validation().dirty()&&!failed.validation().persisted()&&failed.validation().generation()==1,"Uncertain map burn preserves staged data without discard or automatic retry");
        }finally{try(var files=Files.list(directory)){for(Path p:files.toList())Files.delete(p);}Files.delete(directory);}
        System.out.println("Configuration C/Java: exact units, backups/CRC, field diagnostics, pending/stale protection, stage/burn/readback, map preservation and uncertain persistence failure passed. Host model only.");
    }
}
