/* Copyright (c) 2026 Tunuino. Configurable runtime profile; no preset engine. */
package io.tunuino.console;
import io.tunuino.protocol.DeviceIdentity;
import io.tunuino.protocol.EngineOutputClient;
import io.tunuino.protocol.ConfigurationClient;
import java.math.BigDecimal;
import java.nio.ByteBuffer;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.function.Consumer;
import javafx.animation.Timeline;
import javafx.animation.KeyFrame;
import javafx.util.Duration;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import javafx.stage.Stage;

final class EngineControlPane extends VBox {
    private final ConfigurationPane.Access access;
    private final Consumer<EngineOutputClient.State> onState;
    private final Map<String,TextField> fields=new LinkedHashMap<>();
    private final TextField[][] cylinders=new TextField[12][3];
    private final ComboBox<Integer> count=new ComboBox<>();
    private final ComboBox<String> channel=new ComboBox<>();
    private final TextField pulseTime=new TextField();
    private final CheckBox acknowledge=new CheckBox("Confirmo cablagem, polaridade, PMS e limites das cargas ligadas");
    private final Button read=new Button("Ler estado das saídas"),configure=new Button("Enviar perfil desarmado"),
        arm=new Button("Armar controlo do motor"),stop=new Button("PARAR TODAS AS SAÍDAS"),pulse=new Button("Enviar pulso real");
    private final Label result=new Label("Sem estado de saídas recebido da ECU.");
    private EngineOutputClient.State state;
    private DeviceIdentity identity;
    private boolean available,busy;
    private byte[] uploadedProfile;
    private final Timeline refresh;
    EngineControlPane(Stage owner,ConfigurationPane.Access access,Consumer<EngineOutputClient.State> onState){
        this.access=access;this.onState=onState;setSpacing(12);
        setId("engine-control-pane");read.setId("engine-outputs-read");arm.setId("engine-arm");
        stop.setId("engine-stop");configure.setId("engine-configure");pulse.setId("engine-test-pulse");
        result.setId("engine-output-state");result.setWrapText(true);
        for(int i=1;i<=12;++i)count.getItems().add(i);
        count.setPromptText("Seleciona cilindros");count.setId("engine-cylinders");
        GridPane grid=new GridPane();grid.setHgap(12);grid.setVgap(8);int row=0;
        grid.add(new Label("Cilindros"),0,row);grid.add(count,1,row++);
        String[][] definitions={
            {"fuel","ReqFuel de referência (µs)",""},{"dwell","Dwell (µs, máximo 5000)",""},
            {"dead","Tempo morto injetor (µs)",""},{"maximum","Pulso máximo injetor (µs, até 20000)",""},
            {"prime","Escorva da bomba (ms, até 3000)","0"},
            {"fanOn","Ventoinha liga (°C)","95"},{"fanOff","Ventoinha desliga (°C)","90"},
            {"injectionEnd","Fim de injeção relativo ao PMS (°)",""},
            {"toothZero","Ângulo do dente zero, ciclo 720° (°)",""},
            {"pump","Bomba: LS 1–16; 0 sem atribuição","0"},
            {"fan","Ventoinha: LS 1–16; 0 sem atribuição","0"},
            {"relay","Relé principal: LS 1–16; 0 sem atribuição","0"},
            {"camFirst","Janela de fase: primeiro dente",""},
            {"camLast","Janela de fase: último dente",""},
            {"camPhase","Volta de referência da fase (0 ou 1)",""}};
        for(String[] d:definitions){TextField field=new TextField(d[2]);field.setPrefColumnCount(10);
            field.setId("engine-field-"+d[0]);fields.put(d[0],field);grid.add(new Label(d[1]),0,row);grid.add(field,1,row++);}
        GridPane wiring=new GridPane();wiring.setHgap(12);wiring.setVgap(7);
        wiring.addRow(0,new Label("Cilindro"),new Label("Injetor LS 1–16"),new Label("Ignição 1–12"),new Label("PMS no ciclo (°)"));
        for(int i=0;i<12;++i){wiring.add(new Label(Integer.toString(i+1)),0,i+1);
            for(int j=0;j<3;++j){TextField f=new TextField();f.setId("engine-cylinder-"+i+"-"+j);f.setPrefColumnCount(7);cylinders[i][j]=f;wiring.add(f,j+1,i+1);}}
        count.setOnAction(event->{for(int i=0;i<12;++i)for(var f:cylinders[i])f.setDisable(count.getValue()==null||i>=count.getValue());controls();});
        count.getOnAction().handle(null);
        for(int i=1;i<=16;++i)channel.getItems().add("LS "+i);
        for(int i=1;i<=12;++i)channel.getItems().add("IGN "+i);
        channel.setPromptText("Saída de teste");pulseTime.setPromptText("µs");pulseTime.setPrefColumnCount(8);
        getChildren().add(ConsoleShell.card("COMANDO REAL · INJEÇÃO E IGNIÇÃO",
            "Perfil sequencial de quatro tempos, configurável por cilindro. LS comuta cargas para massa; IGN é sinal lógico para uma etapa externa. "
            +"O perfil é volátil e começa desarmado após cada arranque. Não existe configuração predefinida de motor.",
            ConsoleShell.actions(read,stop),result));
        getChildren().add(ConsoleShell.card("PERFIL DO MOTOR",
            "ReqFuel é o tempo de combustível a VE 100% e MAP 100 kPa, antes do tempo morto. "
            +"Configura a captura e guarda a calibração de sensores primeiro. As atribuições não podem repetir saídas.",
            grid,wiring,configure,acknowledge,arm));
        getChildren().add(ConsoleShell.card("ENSAIO DE UMA SAÍDA",
            "Um único pulso, sem rotação detetada e com o controlo do motor desarmado. Limites: LS 20 ms; IGN 5 ms.",
            ConsoleShell.actions(channel,pulseTime,pulse)));
        read.setOnAction(event->run((session,device)->EngineOutputClient.read(session)));
        configure.setOnAction(event->{try{
            byte[] bytes=profile();
            run((session,device)->EngineOutputClient.configure(session,bytes),bytes);
        }catch(IllegalArgumentException error){result.setText(error.getMessage());}});
        arm.setOnAction(event->{
            if(!acknowledge.isSelected()||!matchesProfile())return;
            Alert confirm=ConsoleBranding.alert(Alert.AlertType.CONFIRMATION,
                "Armar permite pulsos reais de injeção/ignição e pode ligar bomba, relé e ventoinha. Confirmas o perfil e as ligações?",ButtonType.OK,ButtonType.CANCEL);
            confirm.initOwner(owner);confirm.setHeaderText("Armar saídas físicas");
            if(confirm.showAndWait().orElse(ButtonType.CANCEL)==ButtonType.OK)
                run((session,device)->{
                    var v=ConfigurationClient.validation(session,device,java.time.Duration.ofSeconds(3));
                    if(v.dirty()||!v.persisted())throw new IllegalStateException("Guarda e valida a calibração antes de armar");
                    return EngineOutputClient.arm(session,v.generation());
                });
        });
        acknowledge.setOnAction(event->controls());
        for(var field:fields.values())field.textProperty().addListener((p,a,b)->{acknowledge.setSelected(false);controls();});
        for(var cylinder:cylinders)for(var field:cylinder)field.textProperty().addListener((p,a,b)->{acknowledge.setSelected(false);controls();});
        stop.setOnAction(event->run((session,device)->EngineOutputClient.stop(session)));
        pulse.setOnAction(event->{try{
            int selected=channel.getSelectionModel().getSelectedIndex();
            int time=number(pulseTime.getText(),0,1,selected<16?20000:5000,"Tempo do pulso");
            if(selected<0)throw new IllegalArgumentException("Seleciona uma saída de teste");
            Alert confirm=ConsoleBranding.alert(Alert.AlertType.CONFIRMATION,
                "Enviar um pulso físico em "+channel.getValue()+" com "+time+" µs?",ButtonType.OK,ButtonType.CANCEL);
            confirm.initOwner(owner);confirm.setHeaderText("Pulso real na carga ligada");
            if(confirm.showAndWait().orElse(ButtonType.CANCEL)==ButtonType.OK)run((session,device)->EngineOutputClient.pulse(session,selected,time));
        }catch(IllegalArgumentException error){result.setText(error.getMessage());}});
        refresh=new Timeline(new KeyFrame(Duration.millis(750),event->{if(available&&!busy&&state!=null&&state.armed())read.fire();}));
        refresh.setCycleCount(Timeline.INDEFINITE);refresh.play();controls();
    }
    private static int number(String text,int decimals,int min,int max,String name){
        try {if(text.length()>32||!text.trim().matches("[+-]?[0-9]+(?:\\.[0-9]+)?"))throw new ArithmeticException();
            int value=new BigDecimal(text.trim()).movePointRight(decimals).intValueExact();
            if(value<min||value>max)throw new ArithmeticException();return value;
        }catch(RuntimeException error){throw new IllegalArgumentException(name+": preenche um valor entre "+BigDecimal.valueOf(min,decimals)+" e "+BigDecimal.valueOf(max,decimals));}
    }
    private int n(String name,int decimals,int min,int max){return number(fields.get(name).getText(),decimals,min,max,name);}
    byte[] profile(){
        if(count.getValue()==null)throw new IllegalArgumentException("Seleciona o número de cilindros");
        ByteBuffer p=ByteBuffer.allocate(128);p.put((byte)1).put((byte)(int)count.getValue());
        p.putShort((short)n("fuel",0,100,30000)).putShort((short)n("dwell",0,100,5000));
        p.putShort((short)n("dead",0,0,5000)).putShort((short)n("maximum",0,100,20000));
        p.putShort((short)n("prime",0,0,3000)).putShort((short)n("fanOn",2,0,12000)).putShort((short)n("fanOff",2,0,12000));
        p.putShort((short)n("injectionEnd",2,-30000,30000)).putInt(n("toothZero",2,0,71999));
        p.put((byte)n("pump",0,0,16)).put((byte)n("fan",0,0,16)).put((byte)n("relay",0,0,16));
        p.put((byte)n("camFirst",0,0,119)).put((byte)n("camLast",0,0,119)).put((byte)n("camPhase",0,0,1));
        p.position(32);long used=0;var angles=new java.util.HashSet<Integer>();
        for(int i=0;i<count.getValue();++i){
            int injector=number(cylinders[i][0].getText(),0,1,16,"Injetor cilindro "+(i+1))-1;
            int coil=number(cylinders[i][1].getText(),0,1,12,"Ignição cilindro "+(i+1))-1;
            int tdc=number(cylinders[i][2].getText(),2,0,71999,"PMS cilindro "+(i+1));
            long bits=(1L<<injector)|(1L<<(16+coil));
            if((used&bits)!=0||!angles.add(tdc))throw new IllegalArgumentException("Saídas ou ângulos PMS repetidos");
            used|=bits;p.put((byte)injector).put((byte)coil).putInt(tdc).putShort((short)0);
        }
        for(String aux:new String[]{"pump","fan","relay"}){int value=n(aux,0,0,16);
            if(value>0){long bit=1L<<(value-1);if((used&bit)!=0)throw new IllegalArgumentException("Saída auxiliar repetida: "+aux);used|=bit;}}
        if(n("fanOff",2,0,12000)>=n("fanOn",2,0,12000)||n("dead",0,0,5000)>=n("maximum",0,100,20000)
            ||n("camFirst",0,0,119)>n("camLast",0,0,119))throw new IllegalArgumentException("Limites de temperatura, injeção ou janela de fase incoerentes");
        return p.array();
    }
    private void run(ConfigurationPane.Operation<EngineOutputClient.State> operation){
        run(operation,null);
    }
    private void run(ConfigurationPane.Operation<EngineOutputClient.State> operation,byte[] sent){
        if(!available||busy)return;busy=true;controls();
        access.run(operation,value->{busy=false;state=value;
            if(sent!=null)uploadedProfile=sent.clone();
            result.setText(value.description()+" · "+value.faultsDescription()
                +" · ativas: "+value.activeChannels()
                +" · injeção "+value.injectionUs()+" µs · perfil "+(value.configured()?value.cylinders()+" cilindros":"por configurar"));
            onState.accept(value);controls();
        },error->{busy=false;state=null;result.setText("Saídas: "+error.getMessage()+". Estado físico por confirmar.");controls();});
    }
    void availability(DeviceIdentity device,boolean connected,boolean blocked){
        if(!java.util.Objects.equals(identity,device)||!connected){identity=device;state=null;uploadedProfile=null;acknowledge.setSelected(false);}
        available=connected&&device!=null&&!blocked;controls();
    }
    private void controls(){boolean idle=available&&!busy;
        boolean armed=state!=null&&state.armed();
        read.setDisable(!idle);stop.setDisable(!idle);configure.setDisable(!idle||armed);
        arm.setDisable(!idle||armed||state==null||!state.configured()||!state.timerReady()||!acknowledge.isSelected()||!matchesProfile());
        pulse.setDisable(!idle||armed);
        count.setDisable(armed||busy);
        for(var field:fields.values())field.setDisable(armed||busy);
        for(int i=0;i<12;++i)for(var field:cylinders[i])field.setDisable(armed||busy||count.getValue()==null||i>=count.getValue());
    }
    private boolean matchesProfile(){try{return uploadedProfile!=null&&java.util.Arrays.equals(uploadedProfile,profile());}catch(IllegalArgumentException error){return false;}}
    void close(){refresh.stop();}
}
