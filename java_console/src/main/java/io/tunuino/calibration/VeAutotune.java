/* Copyright (c) 2026 Tunuino. */
package io.tunuino.calibration;

import io.tunuino.datalog.LiveDataMonitor;
import java.util.Locale;

/** Bounded VE proposals from measured, steady-state wideband data. Never writes or arms an ECU. */
public final class VeAutotune {
    public record Cell(int row,int column,int samples,double before,double after,double changePercent) {}
    private final CalibrationMap base;
    private final long generation;
    private final FuelCorrections fueling;
    private final double target,limit;
    private final int[][] count;
    private final double[][] mean,m2;
    private long previous=-1,steady=-1,running=-1;
    private double rpm,map,tps;
    private boolean invalid;
    private String reason="Aguarda dados reais da ECU";
    private long accepted,rejected;

    public VeAutotune(CalibrationMap base,long generation,FuelCorrections fueling,double target,double maximumPercent) {
        if(base.kind()!=CalibrationMap.Kind.VE||base.rows()!=16||base.columns()!=16||generation<1
            ||target<.65||target>1.30||!Double.isFinite(target)||!Double.isFinite(maximumPercent)||maximumPercent<=0||maximumPercent>5)
            throw new IllegalArgumentException("Base VE, alvo ou limite de correção inválido");
        this.base=base;this.generation=generation;this.fueling=fueling;this.target=target;this.limit=maximumPercent/100;
        count=new int[base.rows()][base.columns()];mean=new double[base.rows()][base.columns()];m2=new double[base.rows()][base.columns()];
    }
    public boolean invalidated(){return invalid;}
    public String reason(){return reason;}
    public long accepted(){return accepted;}
    public long rejected(){return rejected;}
    private void reject(String why){reason=why;rejected++;steady=-1;}
    private boolean warmupUnity(double coolant) {
        if(!fueling.enabled())return true;
        double x=coolant*100;
        double y=fueling.value(11,false);
        for(int i=0;i<2;i++){
            int at=9+4*i;double low=fueling.value(at,true),high=fueling.value(at+4,true);
            if(x>=high)y=fueling.value(at+6,false);
            else if(x>low){y=fueling.value(at+2,false)+(fueling.value(at+6,false)-fueling.value(at+2,false))*(x-low)/(high-low);break;}
            else break;
        }
        return Math.abs(y-10000)<1;
    }
    public void accept(LiveDataMonitor.TuningFrame frame) {
        if(invalid)return;
        var sample=frame.sample();var extra=frame.extras();var feature=extra.sensors();var output=extra.outputs();
        if(output.generation()!=generation||feature.generation()!=generation){invalid=true;reject("Configuração alterada: lê uma nova base VE");return;}
        if(sample.isEmpty()||frame.responseMs()>250){reject("Amostra incompleta ou resposta USB demasiado demorada");return;}
        var s=sample.get();long now=s.timeMs();
        if(previous>=0&&(now<=previous||now-previous>750)){previous=now;running=-1;reject("Interrupção ou relógio sem avanço");return;}
        previous=now;
        if(output.mode()!=2||!output.persisted()||output.faults()!=0){running=-1;reject("Aguarda motor ativo, calibração guardada e ausência de falhas");return;}
        if(running<0)running=now;
        if((s.validMask()&15)!=15||(feature.valid()&33)!=33){reject("RPM, MAP, lambda, CLT e TPS têm de ser válidos");return;}
        long startup=fueling.enabled()?fueling.value(7,false)+1500L:3000L;
        if(s.coolantC()<70||!warmupUnity(s.coolantC())||now-running<startup){reject("Aguarda temperatura e fim das correções de arranque/aquecimento");return;}
        if((feature.state()&~8)!=0||feature.extra()!=0){reject("Corte, enriquecimento transitório ou proteção ativa");return;}
        if((feature.flags()&(256|1024))!=0){reject("Desativa flex fuel e ajustes por cilindro para aprender VE");return;}
        if(s.rpm()<1200||s.rpm()>6000||s.lambda()<.65||s.lambda()>1.30||feature.tps()<500||feature.tps()>8000){reject("Fora da janela de aprendizagem: 1200–6000 rpm e TPS 5–80%");return;}
        if(base.lookup(s.rpm(),s.mapKpa()).clamped()){reject("Leitura fora dos eixos da base VE");return;}
        double measuredTps=feature.tps()/100.0;
        if(steady<0||Math.abs(s.rpm()-rpm)>Math.max(50,rpm*.025)||Math.abs(s.mapKpa()-map)>2||Math.abs(measuredTps-tps)>2){
            steady=now;rpm=s.rpm();map=s.mapKpa();tps=measuredTps;reason="Aguarda 1500 ms em carga estável";rejected++;return;
        }
        if(now-steady<1500){reason="Aguarda 1500 ms em carga estável";rejected++;return;}
        int column=nearest(s.rpm(),true),row=nearest(s.mapKpa(),false);
        if(column<0||row<0){reason="Aguarda um ponto próximo do centro de uma célula";rejected++;return;}
        double wanted=(feature.flags()&4)!=0?feature.target()/1000.0:target;
        if(wanted<.65||wanted>1.30){reject("Alvo lambda inválido");return;}
        // The measured mixture includes the ECU's short-term lambda correction.
        double ratio=s.lambda()/wanted*(1+((feature.state()&8)!=0?feature.trim():0)/10000.0);
        if(!Double.isFinite(ratio)||ratio<.70||ratio>1.30){reject("Desvio de combustível excessivo: verificar calibração e pressão");return;}
        int n=count[row][column];if(n>=10000){reason="Célula com amostras suficientes";return;}
        double value=Math.log(ratio),delta=value-mean[row][column];count[row][column]=++n;
        mean[row][column]+=delta/n;m2[row][column]+=delta*(value-mean[row][column]);accepted++;
        reason="A recolher dados reais · célula "+(row+1)+"/"+(column+1);
    }
    private int nearest(double value,boolean speed) {
        int size=speed?base.columns():base.rows(),best=0;
        for(int i=1;i<size;i++)if(Math.abs(axis(i,speed)-value)<Math.abs(axis(best,speed)-value))best=i;
        double gap=Math.min(best>0?axis(best,speed)-axis(best-1,speed):Double.POSITIVE_INFINITY,
            best+1<size?axis(best+1,speed)-axis(best,speed):Double.POSITIVE_INFINITY);
        return Math.abs(value-axis(best,speed))<=gap*.20?best:-1;
    }
    private double axis(int at,boolean speed){return speed?base.rpmAt(at):base.loadKpaAt(at);}
    public java.util.List<Cell> cells() {
        if(invalid)return java.util.List.of();
        var result=new java.util.ArrayList<Cell>();
        for(int row=0;row<base.rows();row++)for(int col=0;col<base.columns();col++){
            int n=count[row][col];if(n<12||Math.sqrt(m2[row][col]/(n-1))>.03||base.cell(row,col)<=0)continue;
            double correction=Math.max(1-limit,Math.min(1+limit,Math.exp(mean[row][col])));
            double after=Math.round(base.cell(row,col)*correction*100)/100.0;
            // Quantization may otherwise exceed the percentage on very small cells.
            double upper=base.cell(row,col)*(1+limit),lower=base.cell(row,col)*(1-limit);
            if(after>upper)after=Math.floor(upper*100+1e-9)/100.0;
            if(after<lower)after=Math.ceil(lower*100-1e-9)/100.0;
            if(after<=0||after>300||Math.abs(after-base.cell(row,col))<.005)continue;
            result.add(new Cell(row,col,n,base.cell(row,col),after,(after/base.cell(row,col)-1)*100));
        }
        return java.util.List.copyOf(result);
    }
    public CalibrationMap proposal(){
        if(invalid)throw new IllegalStateException("Base alterada: recolha invalidada");
        CalibrationMap map=base;for(var cell:cells())map=map.withCell(cell.row(),cell.column(),String.format(Locale.ROOT,"%.2f",cell.after()));return map;
    }
}
