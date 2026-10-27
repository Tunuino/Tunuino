/* Copyright (c) 2026 Tunuino. Engine calibration only; no serial commands. */
package io.tunuino.console;
import io.tunuino.calibration.FuelCorrections;
import java.math.BigDecimal;
import java.nio.ByteBuffer;
import java.util.LinkedHashMap;
import javafx.scene.control.*;
import javafx.scene.layout.*;
final class FuelCorrectionPane extends VBox {
    private record Field(int offset,int decimals,int minimum,int maximum){}
    private final CheckBox enabled=new CheckBox("Aplicar correções de arranque e aquecimento");
    private final CheckBox fixed=new CheckBox("Usar avanço fixo durante o arranque");
    private final LinkedHashMap<String,Field> definitions=new LinkedHashMap<>();
    private final LinkedHashMap<String,TextField> fields=new LinkedHashMap<>();
    private final LinkedHashMap<String,ComboBox<String>> selectors=new LinkedHashMap<>();
    private boolean blocked;
    private boolean configured;
    private Runnable changed=()->{};
    FuelCorrectionPane(){
        setId("fuel-correction-pane");setSpacing(12);enabled.setId("fuel-corrections-enabled");fixed.setId("fuel-cranking-fixed-advance");
        GridPane grid=new GridPane();grid.setHgap(16);grid.setVgap(9);
        add(grid,"rpm","Arranque abaixo de (rpm)",1,0,100,1000,"200","300","400","500","600");
        add(grid,"cranking","Combustível durante arranque (%)",3,2,5000,50000,"100","125","150","200","250");
        add(grid,"afterstart","Combustível após arranque (%)",5,2,10000,30000,"100","105","110","120","150");
        add(grid,"duration","Duração após arranque (ms)",7,0,0,60000,"0","1000","5000","10000","20000","30000");
        for(int i=0;i<3;i++){
            add(grid,"temperature"+i,"Temperatura de aquecimento "+(i+1)+" (°C)",9+4*i,2,-4000,15000);
            add(grid,"warmup"+i,"Combustível à temperatura "+(i+1)+" (%)",11+4*i,2,5000,50000,"100","110","120","140","160","200");
        }
        add(grid,"advance","Avanço durante arranque (° APMS)",21,2,-1000,2000,"0","5","10","12");
        getChildren().addAll(ConsoleShell.card("ARRANQUE E AQUECIMENTO",
            "100% mantém o combustível base. Durante arranque usa a percentagem de arranque; acima do limite RPM usa a curva de aquecimento e o enriquecimento após arranque, que decresce até 100%. O tempo morto é adicionado no fim. Afina estes valores para o motor real.",enabled,fixed,grid));
        set(new FuelCorrections(new byte[25]));
        enabled.setOnAction(e->{configured=true;refresh();changed.run();});fixed.setOnAction(e->changed.run());
    }
    void onChanged(Runnable action){changed=action;}
    private void add(GridPane grid,String key,String label,int offset,int decimals,int min,int max,String... choices){
        ComboBox<String> combo=new ComboBox<>();combo.setEditable(true);combo.setPrefWidth(150);combo.getItems().addAll(choices);
        TextField input=combo.getEditor();input.setId("fuel-correction-"+key);fields.put(key,input);
        selectors.put(key,combo);
        definitions.put(key,new Field(offset,decimals,min,max));int row=grid.getRowCount();grid.addRow(row,new Label(label),combo);
        input.textProperty().addListener((p,a,b)->changed.run());
        combo.disableProperty().bind(input.disableProperty());
    }
    FuelCorrections candidate(){
        if(!enabled.isSelected()&&!configured)return new FuelCorrections(new byte[25]);
        ByteBuffer p=ByteBuffer.allocate(25);p.put(0,(byte)(enabled.isSelected()?1:0));p.put(23,(byte)(fixed.isSelected()?1:0));
        for(var entry:definitions.entrySet()){
            String text=fields.get(entry.getKey()).getText().trim().replace(',','.');Field f=entry.getValue();
            try{
                if(text.length()>32||!text.matches("[+-]?[0-9]+(?:\\.[0-9]+)?"))throw new ArithmeticException();
                int value=new BigDecimal(text).movePointRight(f.decimals()).intValueExact();
                if(value<f.minimum()||value>f.maximum())throw new ArithmeticException();p.putShort(f.offset(),(short)value);
            }catch(RuntimeException error){throw new IllegalArgumentException("Arranque/aquecimento: valor inválido em "+entry.getKey());}
        }
        FuelCorrections result=new FuelCorrections(p.array());
        if(!result.valid())throw new IllegalArgumentException("Arranque/aquecimento: temperaturas devem crescer; enriquecimento após arranque exige duração em ms");
        return result;
    }
    void set(FuelCorrections value){
        FuelCorrections display=value.configured()?value:FuelCorrections.reference();configured=value.configured();
        for(var entry:definitions.entrySet()){
            Field f=entry.getValue();String text=BigDecimal.valueOf(display.value(f.offset(),f.minimum()<0),f.decimals()).stripTrailingZeros().toPlainString();
            selectors.get(entry.getKey()).setValue(text);fields.get(entry.getKey()).setText(text);
        }
        fixed.setSelected(display.fixedAdvance());enabled.setSelected(value.enabled());refresh();
    }
    void blocked(boolean value){blocked=value;refresh();}
    private void refresh(){enabled.setDisable(blocked);fixed.setDisable(blocked||!enabled.isSelected());fields.values().forEach(f->f.setDisable(blocked||!enabled.isSelected()));}
}
