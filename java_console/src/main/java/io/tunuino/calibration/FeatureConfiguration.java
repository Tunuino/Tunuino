/* Copyright (c) 2026 Tunuino. Optional engine features in page 4. */
package io.tunuino.calibration;
import java.nio.ByteBuffer;
import java.math.BigDecimal;
import java.util.*;
public final class FeatureConfiguration {
    public enum Option {
        IAT("Compensação IAT e atraso por ar quente","Sensores",1),VOLTAGE("Curvas de dead time e dwell por tensão","Tensão",2),
        TARGET("Aplicar alvo lambda por MAP","Lambda",4),LAMBDA("Correção lambda em malha fechada","Lambda",8),ACCEL("Enriquecimento por velocidade TPS","Transientes",16),
        DFCO("Corte de combustível em desaceleração","Transientes",32),LIMIT("Limitador de RPM por corte de combustível","Limites",64),
        LAUNCH("Launch por entrada analógica condicionada","Limites",128),FLEX("Flex fuel por controlador analógico","Flex fuel",256),
        PROTECTION("Corte retido por CLT ou tensão","Limites",512),TRIMS("Aplicar ajustes individuais dos cilindros","Cilindros",1024);
        public final String label,group;public final int bit;Option(String l,String g,int b){label=l;group=g;bit=b;}
    }
    public enum Field {
        TRANSIENT_DELAY("Pausa lambda após transiente (ms)","Lambda",178,0,0,5000,500),
        IAT_INPUT("Entrada para temperatura do ar (IAT)","Sensores",72,0,0,4,0),
        VOLT_INPUT("Entrada para tensão da bateria","Sensores",73,0,0,11,0),
        LAUNCH_INPUT("Entrada para comando de launch","Limites",74,0,0,11,0),
        FLEX_INPUT("Entrada para teor de etanol","Flex fuel",75,0,0,11,0),
        IAT_RAW1("IAT ADC 1","Sensores",76,0,0,4095,400),
        IAT_RAW2("IAT ADC 2","Sensores",78,0,0,4095,2000),
        IAT_RAW3("IAT ADC 3","Sensores",80,0,0,4095,3600),
        IAT_TEMP1("IAT temperatura 1 (°C)","Sensores",82,2,-4000,15000,10000),
        IAT_TEMP2("IAT temperatura 2 (°C)","Sensores",84,2,-4000,15000,2000),
        IAT_TEMP3("IAT temperatura 3 (°C)","Sensores",86,2,-4000,15000,-2000),
        VOLT_RAW_LOW("Tensão ADC mínimo","Sensores",88,0,0,4095,100),
        VOLT_RAW_HIGH("Tensão ADC máximo","Sensores",90,0,0,4095,3995),
        VOLT_LOW("Tensão no mínimo (V)","Sensores",92,2,0,2399,500),
        VOLT_HIGH("Tensão no máximo (V)","Sensores",94,2,1,2400,2000),
        VOLT_AXIS1("Tensão da curva 1 (V)","Tensão",96,2,500,2000,800),
        VOLT_AXIS2("Tensão da curva 2 (V)","Tensão",98,2,500,2000,1200),
        VOLT_AXIS3("Tensão da curva 3 (V)","Tensão",100,2,500,2000,1600),
        DEAD1("Dead time 1 (ms)","Tensão",102,3,0,5000,1500),
        DEAD2("Dead time 2 (ms)","Tensão",104,3,0,5000,1000),
        DEAD3("Dead time 3 (ms)","Tensão",106,3,0,5000,700),
        DWELL1("Dwell 1 (ms)","Tensão",108,3,100,5000,4000),
        DWELL2("Dwell 2 (ms)","Tensão",110,3,100,5000,3000),
        DWELL3("Dwell 3 (ms)","Tensão",112,3,100,5000,2500),
        TARGET_MAP1("MAP alvo 1 (kPa absolutos)","Lambda",114,2,1000,50000,3000),
        TARGET_MAP2("MAP alvo 2 (kPa absolutos)","Lambda",116,2,1000,50000,7000),
        TARGET_MAP3("MAP alvo 3 (kPa absolutos)","Lambda",118,2,1000,50000,10000),
        TARGET_LAMBDA1("Lambda alvo 1","Lambda",120,3,650,1300,1000),
        TARGET_LAMBDA2("Lambda alvo 2","Lambda",122,3,650,1300,1000),
        TARGET_LAMBDA3("Lambda alvo 3","Lambda",124,3,650,1300,900),
        LAMBDA_P("Proporcional (% / lambda)","Lambda",126,2,0,20000,5000),
        LAMBDA_I("Integral (% / lambda / s)","Lambda",128,2,0,20000,500),
        LAMBDA_LIMIT("Correção máxima ± (%)","Lambda",130,2,0,2500,1000),
        LAMBDA_CLT("Temperatura mínima (°C)","Lambda",132,2,0,12000,6000),
        LAMBDA_MIN_RPM("RPM mínimo","Lambda",134,0,500,11999,1200),
        LAMBDA_MAX_RPM("RPM máximo","Lambda",136,0,501,12000,5000),
        LAMBDA_TPS("TPS máximo (%)","Lambda",138,2,0,10000,7000),
        LAMBDA_DELAY("Atraso após arranque (ms)","Lambda",140,0,0,60000,10000),
        ACCEL_RATE("Ativar acima de TPS (% / s)","Transientes",142,2,100,50000,5000),
        ACCEL_ADD("Combustível adicional inicial (ms)","Transientes",144,3,0,5000,1000),
        ACCEL_DURATION("Decaimento de aceleração (ms)","Transientes",146,0,10,2000,200),
        DFCO_CUT("Cortar acima de (rpm)","Transientes",148,0,501,12000,2500),
        DFCO_RESUME("Retomar abaixo de (rpm)","Transientes",150,0,500,11999,1800),
        DFCO_TPS("TPS máximo em desaceleração (%)","Transientes",152,2,0,1000,200),
        DFCO_CLT("Temperatura mínima DFCO (°C)","Transientes",154,2,0,12000,6000),
        DFCO_DELAY("Atraso antes do corte (ms)","Transientes",156,0,0,5000,500),
        LIMIT_CUT("Limitador: corte (rpm)","Limites",158,0,501,12000,6500),
        LIMIT_RESUME("Limitador: retoma (rpm)","Limites",160,0,500,11999,6000),
        LAUNCH_CUT("Launch: corte (rpm)","Limites",162,0,501,12000,3500),
        LAUNCH_RESUME("Launch: retoma (rpm)","Limites",164,0,500,11999,3200),
        LAUNCH_RAW("Launch ativo acima de ADC","Limites",166,0,100,3995,2500),
        FLEX_RAW0("ADC com 0% etanol","Flex fuel",168,0,0,4094,400),
        FLEX_RAW100("ADC com 100% etanol","Flex fuel",170,0,1,4095,3600),
        FLEX_FUEL("Combustível com 100% etanol (%)","Flex fuel",172,2,10000,25000,16000),
        FLEX_ADVANCE("Avanço adicional com 100% etanol (°)","Flex fuel",174,2,-1000,1000,0),
        IAT_REFERENCE("Temperatura de referência VE (°C)","Sensores",176,2,-4000,10000,2000),
        IAT_RETARD_START("Retirar avanço acima de IAT (°C)","Sensores",180,2,0,12000,5000),
        IAT_RETARD("Atraso de ignição (° / 10 °C)","Sensores",182,2,0,500,100),
        PROTECTION_CLT("Cortar por temperatura CLT (°C)","Limites",184,2,8000,15000,11000),
        PROTECTION_MIN_V("Tensão mínima (V)","Limites",186,2,500,1999,900),
        PROTECTION_MAX_V("Tensão máxima (V)","Limites",188,2,501,2000,1600),
        LAUNCH_ADVANCE("Avanço máximo durante launch (°)","Limites",190,2,-1000,2000,1000),
        FUEL_TRIM1("Cilindro 1: correção de combustível (%)","Cilindros",192,2,-2500,2500,0),
        FUEL_TRIM2("Cilindro 2: correção de combustível (%)","Cilindros",194,2,-2500,2500,0),
        FUEL_TRIM3("Cilindro 3: correção de combustível (%)","Cilindros",196,2,-2500,2500,0),
        FUEL_TRIM4("Cilindro 4: correção de combustível (%)","Cilindros",198,2,-2500,2500,0),
        FUEL_TRIM5("Cilindro 5: correção de combustível (%)","Cilindros",200,2,-2500,2500,0),
        FUEL_TRIM6("Cilindro 6: correção de combustível (%)","Cilindros",202,2,-2500,2500,0),
        FUEL_TRIM7("Cilindro 7: correção de combustível (%)","Cilindros",204,2,-2500,2500,0),
        FUEL_TRIM8("Cilindro 8: correção de combustível (%)","Cilindros",206,2,-2500,2500,0),
        FUEL_TRIM9("Cilindro 9: correção de combustível (%)","Cilindros",208,2,-2500,2500,0),
        FUEL_TRIM10("Cilindro 10: correção de combustível (%)","Cilindros",210,2,-2500,2500,0),
        FUEL_TRIM11("Cilindro 11: correção de combustível (%)","Cilindros",212,2,-2500,2500,0),
        FUEL_TRIM12("Cilindro 12: correção de combustível (%)","Cilindros",214,2,-2500,2500,0),
        SPARK_TRIM1("Cilindro 1: correção de avanço (°)","Cilindros",216,2,-1000,1000,0),
        SPARK_TRIM2("Cilindro 2: correção de avanço (°)","Cilindros",218,2,-1000,1000,0),
        SPARK_TRIM3("Cilindro 3: correção de avanço (°)","Cilindros",220,2,-1000,1000,0),
        SPARK_TRIM4("Cilindro 4: correção de avanço (°)","Cilindros",222,2,-1000,1000,0),
        SPARK_TRIM5("Cilindro 5: correção de avanço (°)","Cilindros",224,2,-1000,1000,0),
        SPARK_TRIM6("Cilindro 6: correção de avanço (°)","Cilindros",226,2,-1000,1000,0),
        SPARK_TRIM7("Cilindro 7: correção de avanço (°)","Cilindros",228,2,-1000,1000,0),
        SPARK_TRIM8("Cilindro 8: correção de avanço (°)","Cilindros",230,2,-1000,1000,0),
        SPARK_TRIM9("Cilindro 9: correção de avanço (°)","Cilindros",232,2,-1000,1000,0),
        SPARK_TRIM10("Cilindro 10: correção de avanço (°)","Cilindros",234,2,-1000,1000,0),
        SPARK_TRIM11("Cilindro 11: correção de avanço (°)","Cilindros",236,2,-1000,1000,0),
        SPARK_TRIM12("Cilindro 12: correção de avanço (°)","Cilindros",238,2,-1000,1000,0);
        public final String label,group;public final int offset,decimals,min,max,reference;
        Field(String l,String g,int o,int d,int lo,int hi,int r){label=l;group=g;offset=o;decimals=d;min=lo;max=hi;reference=r;}
    }
    private final byte[] page;
    public FeatureConfiguration(byte[] page){if(page==null||page.length!=256)throw new IllegalArgumentException("Página de funções deve ter 256 bytes");this.page=page.clone();}
    public byte[] bytes(){return page.clone();}public boolean configured(){return page[68]==1;}
    public int flags(){return Short.toUnsignedInt(ByteBuffer.wrap(page).getShort(70));}
    public boolean enabled(Option o){return (flags()&o.bit)!=0;}
    public int raw(Field f){if(f.offset<76)return page[f.offset]&255;int v=ByteBuffer.wrap(page).getShort(f.offset);return f.min<0?v:v&65535;}
    public String value(Field f){return BigDecimal.valueOf(raw(f),f.decimals).stripTrailingZeros().toPlainString();}
    private static void put(byte[] p,Field f,int value){if(f.offset<76)p[f.offset]=(byte)value;else ByteBuffer.wrap(p).putShort(f.offset,(short)value);}
    public static FeatureConfiguration reference(byte[] original){byte[] p=original.clone();Arrays.fill(p,68,256,(byte)0);p[68]=1;for(var f:Field.values())put(p,f,f.reference);return new FeatureConfiguration(p);}
    public List<String> errors(){
        var e=new ArrayList<String>();if(!configured()){for(int i=68;i<256;i++)if(page[i]!=0){e.add("Extensão de funções inválida");break;}return e;}
        if(page[69]!=0||flags()>2047)e.add("Formato de funções inválido");for(int i=240;i<256;i++)if(page[i]!=0){e.add("Reservas de funções alteradas");break;}
        for(var f:Field.values())if(raw(f)<f.min||raw(f)>f.max)e.add(f.label+": fora dos limites");
        for(Field[] axis:new Field[][]{{Field.IAT_RAW1,Field.IAT_RAW2,Field.IAT_RAW3},{Field.VOLT_AXIS1,Field.VOLT_AXIS2,Field.VOLT_AXIS3},{Field.TARGET_MAP1,Field.TARGET_MAP2,Field.TARGET_MAP3}})if(raw(axis[0])>=raw(axis[1])||raw(axis[1])>=raw(axis[2]))e.add("Eixos ADC, tensão e MAP devem crescer");
        if(raw(Field.IAT_TEMP1)==raw(Field.IAT_TEMP2)&&raw(Field.IAT_TEMP2)==raw(Field.IAT_TEMP3))e.add("Curva IAT deve variar");
        for(Field[] pair:new Field[][]{{Field.VOLT_RAW_LOW,Field.VOLT_RAW_HIGH},{Field.VOLT_LOW,Field.VOLT_HIGH},{Field.FLEX_RAW0,Field.FLEX_RAW100},{Field.LAMBDA_MIN_RPM,Field.LAMBDA_MAX_RPM},{Field.DFCO_RESUME,Field.DFCO_CUT},{Field.LIMIT_RESUME,Field.LIMIT_CUT},{Field.LAUNCH_RESUME,Field.LAUNCH_CUT},{Field.PROTECTION_MIN_V,Field.PROTECTION_MAX_V}})if(raw(pair[0])>=raw(pair[1]))e.add("Limites incompatíveis: "+pair[0].label);
        if(raw(Field.LAUNCH_CUT)>raw(Field.LIMIT_CUT))e.add("Launch deve respeitar o limitador principal");
        if(enabled(Option.IAT)&&raw(Field.IAT_INPUT)==0)e.add("IAT exige entrada TEMP");
        if((enabled(Option.VOLTAGE)||enabled(Option.PROTECTION))&&raw(Field.VOLT_INPUT)==0)e.add("Tensão exige entrada AV calibrada");
        if(enabled(Option.LAUNCH)&&(!enabled(Option.LIMIT)||raw(Field.LAUNCH_INPUT)==0))e.add("Launch exige limitador e entrada condicionada");
        if(enabled(Option.FLEX)&&raw(Field.FLEX_INPUT)==0)e.add("Flex fuel exige controlador com saída analógica");
        if(enabled(Option.LAMBDA)&&!enabled(Option.TARGET))e.add("Malha fechada exige alvo lambda");
        if((enabled(Option.ACCEL)||enabled(Option.DFCO)||enabled(Option.LAMBDA))&&(page[5]&255)==0)e.add("Transientes e lambda exigem TPS calibrado");
        return List.copyOf(e.stream().distinct().toList());
    }
    public FeatureConfiguration edited(Map<Field,String> values,Set<Option> options){
        byte[] p=(configured()?this:reference(page)).bytes();p[68]=1;int flags=0;for(var o:options)flags|=o.bit;ByteBuffer.wrap(p).putShort(70,(short)flags);
        for(var entry:values.entrySet()){Field f=entry.getKey();String text=entry.getValue().trim().replace(',','.');try{if(text.length()>32||!text.matches("[+-]?[0-9]+(?:\\.[0-9]+)?"))throw new ArithmeticException();int n=new BigDecimal(text).movePointRight(f.decimals).intValueExact();if(n<f.min||n>f.max)throw new ArithmeticException();put(p,f,n);}catch(RuntimeException x){throw new IllegalArgumentException(f.label+": valor inválido ou precisão excessiva");}}
        var c=new FeatureConfiguration(p);if(!c.errors().isEmpty())throw new IllegalArgumentException(String.join(" · ",c.errors()));return c;
    }
}
