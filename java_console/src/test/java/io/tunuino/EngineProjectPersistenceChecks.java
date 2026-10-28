/* Copyright (c) 2026 Tunuino. Real dispatcher/board-adapter integration, synthetic flash. */
package io.tunuino;
import io.tunuino.calibration.*;
import io.tunuino.protocol.*;
import io.tunuino.transport.*;
import java.nio.file.*;
import java.time.Duration;
import java.util.*;
public final class EngineProjectPersistenceChecks {
    private static void check(boolean b,String why){if(!b)throw new AssertionError(why);}
    public static void main(String[] args)throws Exception {
        var identity=new DeviceIdentity(DeviceIdentity.Family.PROTEUS,DeviceIdentity.McuSeries.STM32_F4,7,0,2,0,15,"010000000000000000000000");
        Path directory=Files.createTempDirectory("tunuino-project-fault-");Duration timeout=Duration.ofSeconds(30);
        try(var session=new PacketSession(new ProcessSerialChannel(List.of(args[0],"--fail-next-save")))){
            var before=EngineProjectClient.read(session,identity,timeout);
            var desired=new EngineProject(before.project().core(),before.project().auxiliary(),
                BaseMaps.create(BaseMaps.Template.FLAT_REFERENCE,CalibrationMap.Kind.VE),
                BaseMaps.create(BaseMaps.Template.FLAT_REFERENCE,CalibrationMap.Kind.IGNITION));
            Path backup=directory.resolve("before-failed-save.tnproj");
            try{EngineProjectClient.apply(session,identity,before,desired,backup,timeout);throw new AssertionError("Failed flash save reported success");}
            catch(java.io.IOException expected){check(expected.getMessage().contains("Confirma por nova leitura"),"Uncertain persistence explicitly requires readback");}
            check(Arrays.equals(EngineProject.read(backup).encode(),before.project().encode()),"Complete previous state was backed up before failure");
            var uncertain=ConfigurationClient.validation(session,identity,timeout);
            check(uncertain.dirty()&&uncertain.generation()==before.validation().generation(),"No automatic discard/retry after requesting flash commit");
            check(session.exchange(new byte[]{'D'},Duration.ofSeconds(3))[0]==0,"Operator can explicitly discard uncommitted values after checking state");
            var retained=EngineProjectClient.read(session,identity,timeout);
            check(Arrays.equals(retained.project().encode(),before.project().encode()),"All active pages survive failed project persistence");
            var saved=EngineProjectClient.apply(session,identity,retained,desired,directory.resolve("before-success.tnproj"),timeout);
            check(Arrays.equals(saved.project().encode(),desired.encode())&&saved.validation().generation()==retained.validation().generation()+1,
                "A new explicit operation commits all pages once after failure");
        }finally{try(var files=Files.list(directory)){for(Path file:files.toList())Files.delete(file);}Files.delete(directory);}
        System.out.println("Complete-project C/Java: flash-save failure, prior-state backup, uncertain-commit handling, active-page preservation and explicit retry/readback passed. Synthetic flash only.");
    }
}
