/* Copyright (c) 2026 Tunuino. Editable starting references, not engine calibrations. */
package io.tunuino.calibration;
public final class BaseMaps {
    private BaseMaps(){}
    public enum Template {
        NATURALLY_ASPIRATED("Base genérica aspirado · por afinar"),
        FLAT_REFERENCE("Referência plana · VE 80% / ignição 10°");
        private final String label;
        Template(String label){this.label=label;}
        @Override public String toString(){return label;}
    }
    public static CalibrationMap create(Template template,CalibrationMap.Kind kind){
        if(template==null||kind==null)throw new IllegalArgumentException("Escolhe um modelo e tipo de mapa");
        int[] rpm={200,400,600,800,1000,1500,2000,2500,3000,3500,4000,4500,5000,5500,6000,7000};
        int[] load={20000,25000,30000,35000,40000,45000,50000,55000,60000,65000,70000,75000,80000,90000,100000,105000};
        int[] fullLoadVe={55,55,55,58,60,65,72,78,82,85,85,82,80,77,74,70};
        int[] fullLoadSpark={5,5,8,10,10,12,15,18,20,22,22,22,22,22,20,18};
        int[][] cells=new int[16][16];
        for(int row=0;row<16;row++)for(int col=0;col<16;col++){
            int value;
            if(template==Template.FLAT_REFERENCE)value=kind==CalibrationMap.Kind.VE?80:10;
            else if(kind==CalibrationMap.Kind.VE)value=Math.max(40,fullLoadVe[col]-Math.max(0,100-load[row]/1000)/5);
            else value=fullLoadSpark[col]+(rpm[col]>=1500?Math.max(0,100-load[row]/1000)/10:0);
            cells[row][col]=value*1000;
        }
        return new CalibrationMap(kind,rpm,load,cells);
    }
    /** Nominal air density 1.184 g/L, gasoline density 0.745 g/mL, AFR 14.7.
     * Injector flow must be specified at the actual differential fuel pressure. */
    public static int gasolineRequiredFuelUs(double displacementCc,int cylinders,double injectorCcMinute){
        if(!Double.isFinite(displacementCc)||!Double.isFinite(injectorCcMinute)||displacementCc<50||displacementCc>20000
            ||cylinders<1||cylinders>12||injectorCcMinute<10||injectorCcMinute>5000)throw new IllegalArgumentException("Cilindrada ou caudal do injetor fora dos limites");
        long value=Math.round(displacementCc/cylinders*0.001184/14.7/(injectorCcMinute*0.745/60)*1_000_000);
        if(value<100||value>30000)throw new IllegalArgumentException("ReqFuel calculado fora de 0,100 a 30,000 ms");
        return (int)value;
    }
}
