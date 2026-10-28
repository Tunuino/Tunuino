/* Copyright (c) 2026 Tunuino. */
package io.tunuino;
import io.tunuino.calibration.*;
import io.tunuino.datalog.*;
import io.tunuino.protocol.*;
import java.util.*;
public final class VeAutotuneChecks {
    private static void check(boolean b,String message){if(!b)throw new AssertionError(message);}
    private static final CalibrationMap BASE=BaseMaps.create(BaseMaps.Template.FLAT_REFERENCE,CalibrationMap.Kind.VE);
    private static LiveDataMonitor.TuningFrame frame(long time,double lambda,int flags,int state,int trim,long generation,int mode,int valid,long latency){
        var f=new AuxControlClient.FeatureState(flags,33,2500,0,0,0,900,10000,trim,0,500,2000,state,0,generation);
        var o=new EngineOutputClient.State(mode,0,0,0,true,4,true,generation,2000,true,true);
        return new LiveDataMonitor.TuningFrame(Optional.of(new LogSample(time,3000,70,lambda,90,valid)),new LiveDataMonitor.Extras(f,o,null),latency);
    }
    private static VeAutotune learner(double limit){return new VeAutotune(BASE,1,new FuelCorrections(new byte[25]),1,limit);}
    public static void main(String[] args){
        var learned=learner(2);
        for(int i=1;i<=120;i++)learned.accept(frame(i*100,1.10,0,0,0,1,2,15,10));
        check(learned.accepted()>12&&learned.cells().size()==1,"Stable measured samples identify one actual map cell");
        var cell=learned.cells().get(0);check(cell.before()==80&&cell.after()==81.6,"Lean mixture increases VE, bounded to two percent");
        var proposed=learned.proposal();int changed=0;
        for(int r=0;r<16;r++)for(int c=0;c<16;c++)if(BASE.cell(r,c)!=proposed.cell(r,c))changed++;
        check(changed==1&&BASE.cell(cell.row(),cell.column())==80,"Proposals preserve original immutable map and all unvisited cells");
        MapWriteClient.encode(proposed); // Native centipercent precision, no upload-time rounding failure.
        var pi=learner(5);for(int i=1;i<=120;i++)pi.accept(frame(i*100,1,0,8,1000,1,2,15,10));
        check(pi.cells().get(0).after()==84,"Short-term PI correction is included even when measured lambda reaches target");
        var target=learner(5);for(int i=1;i<=120;i++)target.accept(frame(i*100,.99,4,0,0,1,2,15,10));
        check(target.cells().get(0).after()==84,"Active ECU target curve takes precedence over fallback lambda");
        for(int scenario=0;scenario<4;scenario++){
            var rejected=learner(5);for(int i=1;i<=120;i++)rejected.accept(frame(i*100,1.1,scenario==2?256:0,scenario==0?1:0,0,1,scenario==1?0:2,15,scenario==3?300:10));
            check(rejected.cells().isEmpty(),"Cuts, disarming, flex fuel and stale USB cannot learn");
        }
        learned.accept(frame(12100,1.1,0,0,0,2,2,15,10));check(learned.invalidated()&&learned.cells().isEmpty(),"Changed generation permanently invalidates stale proposal");
        try{learned.proposal();throw new AssertionError("Invalidated proposal accepted");}catch(IllegalStateException expected){}
        var gap=learner(5);gap.accept(frame(100,1,0,0,0,1,2,15,10));gap.accept(frame(100,1.1,0,0,0,1,2,15,10));check(gap.accepted()==0,"Duplicate ECU timestamps are not additional evidence");
        var warm=new VeAutotune(BASE,1,FuelCorrections.reference(),1,5);
        for(int i=1;i<=90;i++)warm.accept(frame(i*100,1.1,0,0,0,1,2,15,10));
        check(warm.accepted()==0,"Afterstart duration is excluded even with a warm coolant signal");
        System.out.println("VE Autotune: measured-sample filtering, PI and target accounting, per-session bounds, native precision, immutable proposals, disarming/cut/flex/stale exclusions and generation invalidation passed. Synthetic test inputs only.");
    }
}
