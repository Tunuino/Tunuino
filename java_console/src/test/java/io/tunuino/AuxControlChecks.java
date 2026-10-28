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
            MapWriteClient.write(session,d,BaseMaps.create(BaseMaps.Template.FLAT_REFERENCE,CalibrationMap.Kind.VE),timeout);
            var tuningBase=VeAutotuneClient.read(session,d);
            var tuneBackup=java.nio.file.Files.createTempDirectory("tunuino-ve-backup").resolve("base.tnmap");
            var tuneProposal=tuningBase.map().withCell(0,0,"81.60");
            java.nio.file.Files.writeString(tuneBackup,"preserve existing file");
            try{VeAutotuneClient.write(session,d,tuningBase,tuneProposal,tuneBackup);throw new AssertionError("Existing VE backup overwritten");}catch(java.nio.file.FileAlreadyExistsException expected){}
            check(java.nio.file.Files.readString(tuneBackup).equals("preserve existing file")&&ConfigurationClient.validation(session,d,timeout).generation()==tuningBase.generation(),"Backup failure leaves file and ECU generation untouched");
            java.nio.file.Files.delete(tuneBackup);
            VeAutotuneClient.write(session,d,tuningBase,tuneProposal,tuneBackup);
            check(java.util.Arrays.equals(CalibrationMap.read(tuneBackup).encode(),tuningBase.map().encode()),"Autotune preserves the exact previous map before commit");
            check(java.util.Arrays.equals(MapReadClient.read(session,d,CalibrationMap.Kind.VE,timeout).encode(),tuneProposal.encode())&&!EngineOutputClient.read(session).armed(),"Actual board stores bounded VE proposal and readback without arming");
            try{VeAutotuneClient.write(session,d,tuningBase,tuneProposal,tuneBackup.resolveSibling("stale.tnmap"));throw new AssertionError("Stale VE proposal overwritten");}catch(java.io.IOException expected){}
            try{VeAutotuneClient.write(session,d,tuningBase,tuningBase.map().withCell(0,0,"90"),tuneBackup.resolveSibling("unbounded.tnmap"));throw new AssertionError("Unbounded VE proposal accepted");}catch(IllegalArgumentException expected){}
            java.nio.file.Files.delete(tuneBackup);java.nio.file.Files.delete(tuneBackup.getParent());
            var fullBefore=EngineProjectClient.read(session,d,Duration.ofSeconds(30));
            var oldProject=fullBefore.project();byte[] pairProfile=oldProject.core().engineProfile();
            pairProfile[0]=3;pairProfile[1]=2;pairProfile[28]=3;pairProfile[40]=1;pairProfile[41]=pairProfile[33];ByteBuffer.wrap(pairProfile).putInt(42,36000);
            var fullDesired=new EngineProject(oldProject.core().withEngine(pairProfile,false).withFuelCorrections(FuelCorrections.reference()),
                oldProject.auxiliary().edited(Map.of(AuxConfiguration.Field.IDLE_P,"6.25"),1,8,2,9,3),
                oldProject.ve().withCell(0,1,"83.00"),oldProject.ignition().withCell(0,1,"9.50"));
            Path fullDir=Files.createTempDirectory("tunuino-full-project-");Path fullBackup=fullDir.resolve("previous.tnproj");
            Files.writeString(fullBackup,"existing backup");
            try{EngineProjectClient.apply(session,d,fullBefore,fullDesired,fullBackup,Duration.ofSeconds(30));throw new AssertionError("Project backup overwritten");}catch(FileAlreadyExistsException expected){}
            check(Files.readString(fullBackup).equals("existing backup")&&ConfigurationClient.validation(session,d,timeout).generation()==fullBefore.validation().generation(),"Full project backup failure changes no ECU bytes");
            Files.delete(fullBackup);
            var fullSaved=EngineProjectClient.apply(session,d,fullBefore,fullDesired,fullBackup,Duration.ofSeconds(30));
            check(Arrays.equals(fullSaved.project().encode(),fullDesired.encode())&&fullSaved.validation().generation()==fullBefore.validation().generation()+1&&!fullSaved.engine().armed(),"All four pages, including paired profile, commit atomically in one generation and read back without arming");
            check(Arrays.equals(EngineProject.read(fullBackup).encode(),oldProject.encode()),"Complete prior ECU state is recoverable from one CRC-protected file");
            try{EngineProjectClient.apply(session,d,fullBefore,oldProject,fullDir.resolve("stale.tnproj"),Duration.ofSeconds(30));throw new AssertionError("Stale full restore accepted");}catch(java.io.IOException expected){}
            var pairRoutes=IoAssignmentClient.read(session,d,timeout);
            check(pairRoutes.assignments().functions(IoAssignments.Group.IGNITION).size()==2&&pairRoutes.assignments().changes().isEmpty(),"One paired ignition function preserves both cylinder descriptors without false overlap");
            var pairMoved=pairRoutes.assignments().withFunction(IoAssignments.Group.IGNITION,4,1);
            check(pairMoved.core().engineProfile()[33]==4&&pairMoved.core().engineProfile()[41]==4,"Moving one coil moves both paired cylinders");
            byte[] badAux=fullDesired.auxiliary().bytes();ByteBuffer.wrap(badAux).putShort(216,(short)100);
            check(!new AuxConfiguration(badAux).errors(pairProfile).isEmpty(),"Client rejects conflicting ignition trims on the shared coil");
            byte[] trimWrite=ByteBuffer.allocate(9).put((byte)'w').putShort((short)4).putShort((short)216).putShort((short)2).putShort((short)100).array();
            check(session.exchange(trimWrite,timeout)[0]==0,"Staging a trim is not an implicit flash commit");
            check(ConfigurationClient.validation(session,d,timeout).errors()!=0,"Firmware independently refuses inconsistent paired-coil trims");
            check(session.exchange(new byte[]{'D'},timeout)[0]==0,"Discard restores only the pending trim");
            Files.delete(fullBackup);Files.delete(fullDir);
            outputs=EngineOutputClient.read(session);saved=routed.auxiliary();desired=saved.configuration();
            byte[] pendingProject=ByteBuffer.allocate(9).put((byte)'w').putShort((short)1).putShort((short)96).putShort((short)2).putShort((short)11000).array();
            check(session.exchange(pendingProject,timeout)[0]==0,"Pending settings remain independent of project restore");
            try{EngineProjectClient.read(session,d,Duration.ofSeconds(20));throw new AssertionError("Pending project was silently read as active");}catch(java.io.IOException expected){}
            check(ConfigurationClient.validation(session,d,timeout).dirty(),"Project read does not discard another editor's pending values");
            check(session.exchange(new byte[]{'D'},timeout)[0]==0,"User discard returns to the saved project");
            var armedTuneBase=VeAutotuneClient.read(session,d);
            EngineOutputClient.arm(session,outputs.generation());
            try{EngineProjectClient.apply(session,d,fullSaved,oldProject,tuneBackup.resolveSibling("armed-project.tnproj"),Duration.ofSeconds(30));throw new AssertionError("Armed complete restore accepted");}catch(java.io.IOException expected){}
            check(!Files.exists(tuneBackup.resolveSibling("armed-project.tnproj")),"Armed project restore is rejected before backup/staging");
            try{VeAutotuneClient.write(session,d,armedTuneBase,armedTuneBase.map(),tuneBackup.resolveSibling("armed.tnmap"));throw new AssertionError("Autotune wrote while armed");}catch(java.io.IOException expected){}
            check(!java.nio.file.Files.exists(tuneBackup.resolveSibling("armed.tnmap")),"Armed learning commit is rejected before creating files");
            try{IoAssignmentClient.apply(session,d,routed,moved.withFunction(IoAssignments.Group.POWER,10,1),timeout);throw new AssertionError("Armed assignment write accepted");}catch(java.io.IOException expected){}
            try{AuxControlClient.apply(session,d,saved,desired.edited(Map.of(AuxConfiguration.Field.IDLE_P,"6"),1,8,2,9,3),timeout);throw new AssertionError("Running write accepted");}catch(java.io.IOException expected){}
            check(!AuxControlClient.read(session,d,timeout).validation().dirty(),"Running write changes no staged bytes");EngineOutputClient.stop(session);
            var running=new java.util.concurrent.atomic.AtomicBoolean(true);
            var samples=new java.util.concurrent.atomic.AtomicInteger();var extraCount=new java.util.concurrent.atomic.AtomicInteger();
            var monitor=new io.tunuino.datalog.LiveDataMonitor(session,d,Duration.ofMillis(10));
            var tuningFrames=new java.util.concurrent.atomic.AtomicInteger();
            monitor.tuningObserver(frame->{check(frame.sample().isPresent()&&frame.extras().outputs().generation()==frame.extras().sensors().generation(),"Learning frame pairs actual native samples and matching feature/output generation");tuningFrames.incrementAndGet();});
            long count=monitor.run(running::get,sample->{if(samples.incrementAndGet()==3)running.set(false);},error->{throw new AssertionError(error);},extra->{
                check(extra.sensors().flags()==2047&&extra.outputs().configured()&&extra.profile()[0]==3,"Native extended sensors and saved output routing reach the monitor");
                byte[] copy=extra.profile();copy[0]=99;check(extra.profile()[0]==3,"Monitor profile snapshot is immutable");extraCount.incrementAndGet();
            });
            check(count==3&&extraCount.get()>0&&tuningFrames.get()>0&&session.isOpen(),"Extended telemetry shares the serial worker and preserves the session");
        }
        byte[] fault=new byte[24];fault[1]=1;fault[2]=3;fault[3]=16;fault[14]=1;
        check(EngineOutputClient.decode(fault).faultsDescription().contains("turbo"),"Overboost fault decodes explicitly");
        System.out.println("Aux C/Java: exact settings, backups, page-4 persistence, no auto-arm, ownership rejection, pending/stale protection, real-dispatch VE baseline/backup/commit/readback, learning-frame generation, complete four-page project restore/backup/readback and paired-ignition validation passed. Register model only.");
    }
}
