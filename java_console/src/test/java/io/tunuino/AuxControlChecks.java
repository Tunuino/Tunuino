/* Copyright (c) 2026 Tunuino. Actual board protocol, synthetic GPIO/flash. */
package io.tunuino;
import io.tunuino.calibration.*;
import io.tunuino.protocol.*;
import io.tunuino.transport.*;
import java.nio.ByteBuffer;
import java.nio.file.*;
import java.time.Duration;
import java.util.*;
public final class AuxControlChecks {
    private static void check(boolean value,String reason){if(!value)throw new AssertionError(reason);}
    public static void main(String[] args)throws Exception{
        var ref=AuxConfiguration.reference();check(ref.errors(null).isEmpty()&&ref.idleMode()==0&&ref.boostMode()==0,"Reference does not activate valves");
        var features=FeatureConfiguration.reference(ref.bytes());check(features.flags()==0&&features.errors().isEmpty(),"Feature reference stays disabled");
        var precise=features.edited(Map.of(FeatureConfiguration.Field.DEAD1,"1,234"),Set.of());check(precise.raw(FeatureConfiguration.Field.DEAD1)==1234,"Latency uses exact milliseconds");
        try{features.edited(Map.of(FeatureConfiguration.Field.DEAD1,"1.2345"),Set.of());throw new AssertionError("Latency rounded silently");}catch(IllegalArgumentException expected){}
        try{ref.edited(Map.of(AuxConfiguration.Field.IDLE_P,"1.001"),0,0,0,0,0);throw new AssertionError("PI gain rounded silently");}catch(IllegalArgumentException expected){}
        var decimal=ref.edited(Map.of(AuxConfiguration.Field.IDLE_P,"1,25"),0,0,0,0,0);check(decimal.raw(AuxConfiguration.Field.IDLE_P)==125,"Exact decimal percentage");
        Path dir=Files.createTempDirectory("tunuino-aux-");Path file=dir.resolve("settings.tnaux");
        try{ref.writeNew(file);check(Arrays.equals(ref.bytes(),AuxConfiguration.read(file).bytes()),"Auxiliary backup roundtrip");try{ref.writeNew(file);throw new AssertionError("Backup overwritten");}catch(java.io.IOException expected){}
            byte[] corrupt=Files.readAllBytes(file);corrupt[80]^=1;Files.write(file,corrupt);try{AuxConfiguration.read(file);throw new AssertionError("CRC accepted");}catch(java.io.IOException expected){}
        }finally{Files.deleteIfExists(file);Files.deleteIfExists(dir);}
        try(var session=new PacketSession(new ProcessSerialChannel(List.of(args[0])))){
            var d=new DeviceIdentity(DeviceIdentity.Family.PROTEUS,DeviceIdentity.McuSeries.STM32_F4,7,0,2,0,15,"010000000000000000000000");Duration timeout=Duration.ofSeconds(20);
            var core=ConfigurationClient.read(session,d,timeout);
            byte[] profile=new byte[128];profile[0]=profile[1]=1;profile[24]=16;profile[25]=1;profile[26]=2;
            ByteBuffer b=ByteBuffer.wrap(profile);b.putShort(2,(short)3000).putShort(4,(short)2000).putShort(6,(short)500).putShort(8,(short)15000).putShort(12,(short)9500).putShort(14,(short)9000);
            var configuration=core.configuration().edited(Map.of(IoConfiguration.Field.MAP_HIGH,"250",IoConfiguration.Field.TEMP_1,"100",IoConfiguration.Field.TEMP_2,"20",IoConfiguration.Field.TEMP_3,"-20"),true,1,0,1).withEngine(profile,false);
            ConfigurationClient.apply(session,d,core,configuration,timeout);
            var initial=AuxControlClient.read(session,d,timeout);check(!initial.configuration().configured(),"Migrated/default page starts inactive");
            var desired=ref.edited(Map.of(),1,8,2,9,3);check(desired.errors(profile).isEmpty(),"Distinct LS assignments accepted");
            var saved=AuxControlClient.apply(session,d,initial,desired,timeout);check(Arrays.equals(saved.configuration().bytes(),desired.bytes())&&!saved.validation().dirty()&&saved.validation().persisted(),"Page 4 stage/burn/readback");
            var state=AuxControlClient.state(session);check(state.idleMode()==1&&state.boostMode()==2&&state.configured()&&!state.tpsValid()&&state.idleDuty()==0&&state.boostDuty()==0,"Actual board loads settings without energizing PWM");
            var outputs=EngineOutputClient.read(session);check(!outputs.armed()&&outputs.active()==0,"Saving never arms outputs");
            try{AuxControlClient.apply(session,d,initial,desired,timeout);throw new AssertionError("Stale auxiliary snapshot accepted");}catch(java.io.IOException expected){}
            var collision=desired.edited(Map.of(),1,1,2,9,3);check(!collision.errors(profile).isEmpty(),"Client detects injector collision with profile");
            try{AuxControlClient.apply(session,d,saved,collision,timeout);throw new AssertionError("Firmware accepted injector collision");}catch(java.io.IOException expected){}
            check(!AuxControlClient.read(session,d,timeout).validation().dirty(),"Rejected collision is discarded before flash");
            var lambdaCore=ConfigurationClient.read(session,d,timeout);
            ConfigurationClient.apply(session,d,lambdaCore,lambdaCore.configuration().edited(Map.of(IoConfiguration.Field.LAMBDA_LOW,"0.5",IoConfiguration.Field.LAMBDA_HIGH,"1.5"),true,1,2,1),timeout);
            var beforeFeatures=AuxControlClient.read(session,d,timeout);
            var full=new FeatureConfiguration(FeatureConfiguration.reference(beforeFeatures.configuration().bytes()).bytes()).edited(
                Map.of(FeatureConfiguration.Field.IAT_INPUT,"2",FeatureConfiguration.Field.VOLT_INPUT,"4",FeatureConfiguration.Field.LAUNCH_INPUT,"5",FeatureConfiguration.Field.FLEX_INPUT,"6"),
                EnumSet.allOf(FeatureConfiguration.Option.class));
            var withFeatures=AuxControlClient.apply(session,d,beforeFeatures,new AuxConfiguration(full.bytes()),timeout);
            check(withFeatures.configuration().idleMode()==1&&withFeatures.configuration().boostMode()==2,"Feature edit preserves valves and TPS");
            var fs=AuxControlClient.features(session);check(fs.flags()==2047&&fs.multiplier()==0&&!EngineOutputClient.read(session).armed(),"Actual board loads eleven strategies without auto-arm");
            var invalid=full.edited(Map.of(FeatureConfiguration.Field.VOLT_INPUT,"1"),EnumSet.allOf(FeatureConfiguration.Option.class));
            try{AuxControlClient.apply(session,d,withFeatures,new AuxConfiguration(invalid.bytes()),timeout);throw new AssertionError("MAP/battery input collision accepted");}catch(java.io.IOException expected){}
            check(!AuxControlClient.read(session,d,timeout).validation().dirty(),"Rejected sensor collision is discarded");
            var assignments=IoAssignmentClient.read(session,d,timeout);var routes=assignments.assignments();
            check(Arrays.equals(routes.core().bytes(),assignments.core().configuration().bytes())&&Arrays.equals(routes.auxiliary().bytes(),assignments.auxiliary().configuration().bytes()),"Function view preserves the exact configuration when unchanged");
            check(routes.functions(IoAssignments.Group.TEMPERATURE).size()==3&&routes.functions(IoAssignments.Group.IGNITION).size()==2,"Selectors offer only compatible functions and configured cylinders");
            check(!routes.withFunction(IoAssignments.Group.POWER,0,0).errors().isEmpty(),"A required injector cannot silently disappear from the profile");
            try{routes.withFunction(IoAssignments.Group.TEMPERATURE,0,13);throw new AssertionError("Relay assigned to temperature input");}catch(IllegalArgumentException expected){}
            var moved=routes.withFunction(IoAssignments.Group.ANALOG,6,1).withFunction(IoAssignments.Group.POWER,9,1).withFunction(IoAssignments.Group.TEMPERATURE,2,2);
            check(moved.function(IoAssignments.Group.ANALOG,0)==0&&moved.function(IoAssignments.Group.POWER,0)==0&&moved.errors().isEmpty(),"Moving a function clears its previous terminal without duplicating it");
            var routed=IoAssignmentClient.apply(session,d,assignments,moved,timeout);
            check(routed.core().configuration().mapChannel()==7&&(routed.core().configuration().engineProfile()[32]&255)==9&&(routed.auxiliary().configuration().bytes()[72]&255)==3,"Actual board commits MAP, injector and IAT assignments in both pages");
            check(routed.core().validation().generation()==assignments.core().validation().generation()+1&&!routed.engine().armed()&&routed.core().configuration().value(IoConfiguration.Field.MAP_HIGH).equals("250"),"One atomic generation, calibrated values retained, never auto-arm");
            try{IoAssignmentClient.apply(session,d,assignments,routes,timeout);throw new AssertionError("Stale assignment snapshot accepted");}catch(java.io.IOException expected){}
            outputs=EngineOutputClient.read(session);saved=routed.auxiliary();desired=saved.configuration();
            EngineOutputClient.arm(session,outputs.generation());
            try{IoAssignmentClient.apply(session,d,routed,moved.withFunction(IoAssignments.Group.POWER,10,1),timeout);throw new AssertionError("Armed assignment write accepted");}catch(java.io.IOException expected){}
            try{AuxControlClient.apply(session,d,saved,desired.edited(Map.of(AuxConfiguration.Field.IDLE_P,"6"),1,8,2,9,3),timeout);throw new AssertionError("Running write accepted");}catch(java.io.IOException expected){}
            check(!AuxControlClient.read(session,d,timeout).validation().dirty(),"Running write changes no staged bytes");EngineOutputClient.stop(session);
        }
        byte[] fault=new byte[24];fault[1]=1;fault[2]=3;fault[3]=16;fault[14]=1;
        check(EngineOutputClient.decode(fault).faultsDescription().contains("turbo"),"Overboost fault decodes explicitly");
        System.out.println("Aux C/Java: exact settings, backups, page-4 persistence, no auto-arm, ownership rejection, pending/stale protection and running-write rejection passed. Register model only.");
    }
}
