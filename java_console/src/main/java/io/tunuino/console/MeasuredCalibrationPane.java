/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.calibration.SensorCalibration;
import io.tunuino.protocol.IoClient;
import java.time.Duration;
import java.util.ArrayList;
import java.util.Objects;
import java.util.function.Consumer;
import java.util.function.IntSupplier;
import java.util.function.Supplier;
import javafx.beans.value.ObservableBooleanValue;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.TextField;
import javafx.scene.control.TitledPane;
import javafx.scene.layout.VBox;

/** Guided capture prepares local fields; existing per-sensor save paths persist them. */
final class MeasuredCalibrationPane extends TitledPane {
    private final ConfigurationPane.Access access;
    private final IntSupplier channel;
    private final Supplier<Object> context;
    private final ObservableBooleanValue blocked;
    private final Consumer<SensorCalibration.MeasuredPoint[]> apply;
    private final String[] instructions;
    private final double[] references,values;
    private final int[] raw;
    private final double minimum,maximum;
    private final boolean temperature,tps;
    private final Label instruction=new Label(),message=new Label();
    private final TextField reference=new TextField();
    private final Button capture=new Button("Capturar e avançar"),prepare=new Button("Usar calibração na edição"),restart=new Button("Recomeçar");
    private int step;
    private boolean reading;
    private Object measuredContext;
    private long generation=-1;

    MeasuredCalibrationPane(String id,String name,String note,String[] instructions,double[] references,
            double minimum,double maximum,boolean temperature,boolean tps,
            ConfigurationPane.Access access,IntSupplier channel,Supplier<Object> context,
            ObservableBooleanValue blocked,Consumer<SensorCalibration.MeasuredPoint[]> apply) {
        super("Autocalibração assistida · "+name,null);
        this.access=access;this.channel=channel;this.context=context;this.blocked=blocked;this.apply=apply;
        this.instructions=instructions.clone();this.references=references.clone();this.minimum=minimum;this.maximum=maximum;
        this.temperature=temperature;this.tps=tps;values=new double[references.length];raw=new int[references.length];
        setId(id);setExpanded(false);setAnimated(false);
        instruction.setWrapText(true);message.setWrapText(true);Label help=new Label(note);help.setWrapText(true);
        reference.setId(id+"-reference");reference.setPrefColumnCount(8);reference.setMaxWidth(140);
        reference.setVisible(!tps);reference.setManaged(!tps);
        capture.setId(id+"-capture");prepare.setId(id+"-apply");restart.setId(id+"-restart");message.setId(id+"-result");
        capture.setOnAction(e->capture());restart.setOnAction(e->reset());prepare.setOnAction(e->{
            if(blocked.get()||reading||step!=raw.length)return;
            if(!Objects.equals(measuredContext,context.get())){reset();message.setText("A ECU, ligação ou configuração mudou. Repete a captura.");return;}
            try{apply.accept(SensorCalibration.curve(raw,values,tps));message.setText("Calibração preparada na edição. Revê e usa Guardar calibração para gravar na ECU.");}
            catch(IllegalArgumentException error){message.setText(error.getMessage());}
        });
        setContent(new VBox(8,help,instruction,reference,ConsoleShell.actions(capture,prepare,restart),message));
        blocked.addListener((p,a,b)->controls());reset();
    }
    private void reset() {
        if(reading)return;
        step=0;generation=-1;measuredContext=null;message.setText("Lê a configuração da ECU e escolhe a ligação antes de medir. A captura não grava nem ativa saídas.");
        next();
    }
    private void next() {
        instruction.setText(step==raw.length?"Todos os pontos foram medidos. Revê e prepara a calibração.":
            "Passo "+(step+1)+" / "+raw.length+" · "+instructions[step]);
        if(step<raw.length)reference.setText(Double.toString(references[step]));
        controls();
    }
    private void controls() {
        boolean disabled=blocked.get()||reading;
        capture.setDisable(disabled||step==raw.length);prepare.setDisable(disabled||step!=raw.length);
        restart.setDisable(reading);reference.setDisable(disabled||step==raw.length);
    }
    private void capture() {
        if(blocked.get()||reading||step==raw.length)return;
        final double physical;
        try {
            physical=tps?references[step]:Double.parseDouble(reference.getText().trim().replace(',','.'));
            if(!Double.isFinite(physical)||physical<minimum||physical>maximum)
                throw new IllegalArgumentException("Referência fora dos limites: "+minimum+" a "+maximum);
        }catch(RuntimeException error){message.setText("Indica o valor físico conhecido. "+error.getMessage());return;}
        int selected=channel.getAsInt();
        if(selected==0){message.setText("Escolhe primeiro a ligação do sensor.");return;}
        Object expected=context.get();
        if(step>0&&!Objects.equals(measuredContext,expected)){reset();message.setText("A ligação ou configuração mudou. Recomeça pelo primeiro ponto.");return;}
        reading=true;controls();message.setText("A recolher oito amostras; mantém o sensor estável…");
        access.run((session,device)->{
            var samples=new ArrayList<IoClient.Inputs>();
            for(int i=0;i<8;i++){if(i>0)Thread.sleep(40);samples.add(IoClient.read(session,device,Duration.ofSeconds(2)));}
            return SensorCalibration.stable(samples,selected,temperature);
        },value->{
            reading=false;
            if(!Objects.equals(expected,context.get())||step>0&&generation!=value.generation()) {
                reset();message.setText("A ECU, ligação ou geração mudou durante a captura. Repete a calibração.");return;
            }
            measuredContext=expected;generation=value.generation();raw[step]=value.adc();values[step]=physical;step++;
            next();message.setText("Ponto "+step+" medido: ADC "+value.adc()+" → "+physical+". "+
                (step<raw.length?"Prepara o próximo ponto.":"Usa a calibração na edição antes de guardar."));
        },error->{reading=false;controls();message.setText("Captura recusada: "+error.getMessage());});
    }
}
