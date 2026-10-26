/* Copyright (c) 2026 Tunuino. Structural models, no physical ECU. */
package io.tunuino;
import io.tunuino.calibration.*;
import java.util.*;
public final class EnginePresetChecks {
    private static void check(boolean value,String message){if(!value)throw new AssertionError(message);}
    public static void main(String[] args){
        var four=EnginePreset.choices().stream().filter(p->p.firingOrder().equals(List.of(1,3,4,2))).findFirst().orElseThrow();
        check(four.tdcCentidegrees(1)==0&&four.tdcCentidegrees(3)==18000
            &&four.tdcCentidegrees(4)==36000&&four.tdcCentidegrees(2)==54000,"Firing order maps cylinder numbers to cycle phase");
        for(var preset:EnginePreset.choices()){
            Set<Integer> phases=new HashSet<>();for(int i=1;i<=preset.cylinders();i++){
                int phase=preset.tdcCentidegrees(i);check(phase>=0&&phase<72000&&phases.add(phase),"Distinct 720-degree phases");
            }
            check(preset.cylinders()+3<=16,"Templates leave separate auxiliary outputs");
        }
        try{new EnginePreset("Invalid",List.of(1,1));throw new AssertionError("Repeated cylinder accepted");}catch(IllegalArgumentException expected){}
        try{new EnginePreset("Invalid",List.of(1,3));throw new AssertionError("Missing cylinder accepted");}catch(IllegalArgumentException expected){}
        var draft=IoConfiguration.localDefaults();check(draft.errors().isEmpty(),"Offline draft validates");
        check(!draft.triggerEnabled()&&draft.mapChannel()==0&&draft.lambdaChannel()==0&&draft.coolantChannel()==0,"Draft does not enable unknown sensors or capture");
        System.out.println("Engine templates: firing order, cycle phases, dedicated auxiliary slots and disabled offline sensor defaults passed");
    }
}
