/* Copyright (c) 2026 Tunuino. Measured ADC points and explicit NTC component models. */
package io.tunuino.calibration;
import io.tunuino.protocol.IoClient;

public final class SensorCalibration {
    private SensorCalibration() {}
    public record Point(int adc, double temperature) {}
    public record Reading(int adc,long generation) {}
    public record MeasuredPoint(int adc,double value) {}
    /** A short measured burst, never a guessed 0–5 V endpoint. */
    public static Reading stable(java.util.List<IoClient.Inputs> samples,int channel,boolean temperature) {
        if(samples==null||samples.size()!=8||samples.get(0)==null)throw new IllegalArgumentException("São necessárias oito amostras ADC");
        int[] values=new int[8];long generation=samples.get(0).generation(),previous=-1;
        for(int i=0;i<8;i++) {
            var sample=samples.get(i);values[i]=measured(sample,channel,temperature);
            if(sample.generation()!=generation||sample.sample().timeMs()<=previous)
                throw new IllegalArgumentException("A configuração ou o relógio mudou durante a captura; repete a medição");
            previous=sample.sample().timeMs();
        }
        java.util.Arrays.sort(values);
        if(values[0]<=0||values[7]>=4095)throw new IllegalArgumentException("ADC no limite da escala; verifica a ligação do sensor");
        if(values[7]-values[0]>20)throw new IllegalArgumentException("Sinal instável; mantém o sensor no ponto indicado e repete");
        return new Reading((values[3]+values[4]+1)/2,generation);
    }
    /** TPS retains closed/open order; other curves follow increasing ADC. */
    public static MeasuredPoint[] curve(int[] adc,double[] values,boolean tps) {
        if(adc==null||values==null||adc.length!=values.length||adc.length<2||adc.length>3)
            throw new IllegalArgumentException("Mede todos os pontos da calibração");
        var points=new MeasuredPoint[adc.length];
        for(int i=0;i<adc.length;i++) {
            if(adc[i]<=0||adc[i]>=4095||!Double.isFinite(values[i]))throw new IllegalArgumentException("Ponto de calibração inválido");
            points[i]=new MeasuredPoint(adc[i],values[i]);
        }
        if(!tps)java.util.Arrays.sort(points,java.util.Comparator.comparingInt(MeasuredPoint::adc));
        for(int i=1;i<points.length;i++)if(Math.abs(points[i].adc()-points[i-1].adc())<32
                ||points[i].value()==points[i-1].value())
            throw new IllegalArgumentException("Os pontos devem ter valores diferentes e pelo menos 32 contagens ADC de separação");
        if(points.length==3&&(points[1].value()-points[0].value())*(points[2].value()-points[1].value())<=0)
            throw new IllegalArgumentException("A curva medida deve ser monotónica");
        return points;
    }
    public static int measured(IoClient.Inputs input, int channel, boolean temperature) {
        if(input==null||(input.status()&1)==0)throw new IllegalArgumentException("Leitura ADC indisponível; nenhum ponto foi alterado");
        if(channel<1||channel>(temperature?4:11))throw new IllegalArgumentException("Escolhe primeiro a ligação do sensor");
        int n=input.raw()[temperature?11+channel:channel-1];
        if(n<0||n>4095)throw new IllegalArgumentException("Leitura ADC fora dos limites");
        return n;
    }
    public static AuxConfiguration tps(AuxConfiguration original,java.util.Map<AuxConfiguration.Field,String> values,int channel){
        for(var f:values.keySet())if(f!=AuxConfiguration.Field.TPS_CLOSED&&f!=AuxConfiguration.Field.TPS_OPEN&&f!=AuxConfiguration.Field.TPS_TOLERANCE)
            throw new IllegalArgumentException("Este assistente altera apenas a calibração TPS");
        var start=original.configured()?original:AuxConfiguration.reference();
        return start.edited(values,start.idleMode(),start.idleChannel(),start.boostMode(),start.boostChannel(),channel);
    }
    public static AuxConfiguration sensor(AuxConfiguration original,String kind,java.util.Map<FeatureConfiguration.Field,String> values){
        if(!kind.equals("IAT")&&!kind.equals("VOLT"))throw new IllegalArgumentException("Sensor não suportado");
        for(var f:values.keySet())if(!f.group.equals("Sensores")||!f.name().startsWith(kind+"_"))
            throw new IllegalArgumentException("Este assistente altera apenas a calibração do sensor selecionado");
        var page=new FeatureConfiguration(original.configured()?original.bytes():AuxConfiguration.reference().bytes());
        var enabled=java.util.EnumSet.noneOf(FeatureConfiguration.Option.class);
        for(var option:FeatureConfiguration.Option.values())if(page.enabled(option))enabled.add(option);
        return new AuxConfiguration(page.edited(values,enabled).bytes());
    }
    /** NTC to ground and pull-up to the ADC reference supply; 12-bit full scale. */
    public static Point[] ntc(double resistance25, double beta, double pullup, double... temperatures) {
        if(!Double.isFinite(resistance25)||resistance25<100||resistance25>1000000
            ||!Double.isFinite(beta)||beta<1000||beta>6000||!Double.isFinite(pullup)||pullup<100||pullup>1000000
            ||temperatures.length!=3)throw new IllegalArgumentException("Confirma R25, Beta e resistência de pull-up do circuito");
        Point[] p=new Point[3];
        for(int i=0;i<3;i++){
            double t=temperatures[i];if(!Double.isFinite(t)||t<-40||t>150)throw new IllegalArgumentException("Temperatura entre −40 e 150 °C");
            double r=resistance25*Math.exp(beta*(1/(t+273.15)-1/298.15));
            p[i]=new Point((int)Math.round(4095*r/(pullup+r)),t);
        }
        java.util.Arrays.sort(p,java.util.Comparator.comparingInt(Point::adc));
        if(p[0].adc()<=0||p[2].adc()>=4095||p[0].adc()==p[1].adc()||p[1].adc()==p[2].adc())
            throw new IllegalArgumentException("Os três pontos devem produzir leituras ADC distintas, dentro da escala");
        return p;
    }
}
