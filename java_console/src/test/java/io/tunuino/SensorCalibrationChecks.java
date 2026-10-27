/* Copyright (c) 2026 Tunuino. Calibration math and raw input provenance. */
package io.tunuino;
import io.tunuino.calibration.SensorCalibration;
import io.tunuino.calibration.IoConfiguration;
import io.tunuino.calibration.AuxConfiguration;
import io.tunuino.calibration.FeatureConfiguration;
import io.tunuino.protocol.IoClient;
import io.tunuino.datalog.LogSample;
import java.util.Map;
public final class SensorCalibrationChecks {
    private static void check(boolean c,String m){if(!c)throw new AssertionError(m);}
    private static void reject(Runnable r){try{r.run();throw new AssertionError("Invalid calibration accepted");}catch(IllegalArgumentException expected){}}
    public static void main(String[] args){
        int[] adc=new int[16];for(int i=0;i<adc.length;i++)adc[i]=100+i;
        var input=new IoClient.Inputs(new LogSample(0,0,0,0,0,0),1,3300,1,0,0,adc);
        check(SensorCalibration.measured(input,2,false)==101,"TPS uses the selected AV input");
        check(SensorCalibration.measured(input,3,true)==114,"CLT uses the selected temperature input, not a voltage input");
        reject(()->SensorCalibration.measured(input,0,false));reject(()->SensorCalibration.measured(input,5,true));
        reject(()->SensorCalibration.measured(new IoClient.Inputs(input.sample(),0,3300,1,0,0,adc),2,false));
        var curve=SensorCalibration.ntc(10000,3950,10000,-20,25,120);
        check(curve[1].adc()==2048&&curve[1].temperature()==25,"Equal resistors produce half scale at the rated temperature");
        check(curve[0].temperature()==120&&curve[2].temperature()==-20,"NTC curve is sorted by ADC with decreasing temperature");
        reject(()->SensorCalibration.ntc(10000,3950,10000,25,25,120));
        reject(()->SensorCalibration.ntc(Double.NaN,3950,10000,-20,25,120));
        reject(()->SensorCalibration.ntc(10000,3950,0,-20,25,120));
        var config=IoConfiguration.localDefaults().edited(Map.of(IoConfiguration.Field.MAP_LOW,"0,50"),false,0,0,0);
        check(config.value(IoConfiguration.Field.MAP_LOW).equals("0.5"),"Portuguese decimal entry preserves exact calibration units");
        var start=new AuxConfiguration(FeatureConfiguration.reference(AuxConfiguration.reference().bytes()).edited(Map.of(),java.util.Set.of(FeatureConfiguration.Option.LIMIT)).bytes());
        var tps=SensorCalibration.tps(start,Map.of(AuxConfiguration.Field.TPS_CLOSED,"3200",AuxConfiguration.Field.TPS_OPEN,"800"),2);
        check(tps.raw(AuxConfiguration.Field.TPS_CLOSED)==3200&&tps.raw(AuxConfiguration.Field.TPS_OPEN)==800,"Reverse TPS endpoints retained");
        for(int i=0;i<256;i++)if(i!=5&&i!=6&&i!=7&&i!=8&&i!=9&&i!=66&&i!=67)check(start.bytes()[i]==tps.bytes()[i],"TPS calibration preserves all other bytes at "+i);
        var iat=SensorCalibration.sensor(tps,"IAT",Map.of(FeatureConfiguration.Field.IAT_INPUT,"2",FeatureConfiguration.Field.IAT_TEMP2,"35,5"));
        check(new FeatureConfiguration(iat.bytes()).flags()==new FeatureConfiguration(tps.bytes()).flags(),"Sensor calibration does not enable or disable strategies");
        for(int i=0;i<256;i++)if(i!=72&&i!=84&&i!=85)check(iat.bytes()[i]==tps.bytes()[i],"IAT calibration preserves all other bytes at "+i);
        reject(()->SensorCalibration.tps(start,Map.of(AuxConfiguration.Field.BOOST_TARGET,"180"),2));
        reject(()->SensorCalibration.sensor(start,"IAT",Map.of(FeatureConfiguration.Field.LIMIT_CUT,"9000")));
        System.out.println("Sensor calibration: input provenance, invalid ADC, NTC points and exact comma decimals passed");
    }
}
