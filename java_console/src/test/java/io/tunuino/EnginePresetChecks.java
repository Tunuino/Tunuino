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
        int[][] expected={{0,1,13},{0,3,24},{0,2,28},{1,1,20},{1,2,30},{2,13,20},{2,14,9},{2,15,21}};
        for(int[] e:expected){var group=IoAssignments.Group.values()[e[0]];var route=ProteusRouting.recommendation(group,e[1]);check(route.conventional()&&ProteusRouting.contact(group,route.channel())==e[2],"Conventional connector contact matches independently specified pinout");}
        check(!ProteusRouting.recommendation(IoAssignments.Group.POWER,16).conventional(),"Generic PWM proposal is not presented as a dedicated factory route");
        for(var group:IoAssignments.Group.values()){Set<Integer> contacts=new HashSet<>();for(int i=1;i<=group.count;i++)check(contacts.add(ProteusRouting.contact(group,i)),"Connector contacts are unique within each output/input family");}
        var draft=IoConfiguration.localDefaults();check(draft.errors().isEmpty(),"Offline draft validates");
        check(!draft.triggerEnabled()&&draft.mapChannel()==0&&draft.lambdaChannel()==0&&draft.coolantChannel()==0,"Draft does not enable unknown sensors or capture");
        System.out.println("Engine templates: firing order, cycle phases, dedicated auxiliary slots and disabled offline sensor defaults passed");
    }
}
