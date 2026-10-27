/* Copyright (c) 2026 Tunuino. Explicit engine profile and dedicated actuator-test workspace. */
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
    private final Map<String,ComboBox<String>> selectors=new LinkedHashMap<>();
    private final TextField[][] cylinders=new TextField[12][3];
    private final Map<String,ComboBox<String>> cylinderSelectors=new LinkedHashMap<>();
    private final GridPane wiring=new GridPane();
    private final ComboBox<Integer> count=new ComboBox<>();
    private final ComboBox<io.tunuino.calibration.EnginePreset> presets=new ComboBox<>();
    private final Button applyPreset=new Button("Aplicar modelo à edição");
    private final OutputTestPane tests;
    private final CheckBox acknowledge=new CheckBox("Confirmo cablagem, polaridade, PMS e limites das cargas ligadas");
    private final CheckBox autonomous=new CheckBox("Funcionamento autónomo: ativar o perfil guardado ao ligar a ECU");
    private final Button load=new Button("Ler configuração do motor");
    private final Button read=new Button("Ler estado das saídas"),configure=new Button("Guardar motor na ECU"),
        arm=new Button("Armar controlo do motor"),stop=new Button("PARAR TODAS AS SAÍDAS");
    private final Label result=new Label("Sem estado de saídas recebido da ECU.");
    private final Label presetSummary=new Label("Seleciona um modelo estrutural ou configura os cilindros manualmente.");
    private EngineOutputClient.State state;
    private DeviceIdentity identity;
    private boolean available,busy;
    private byte[] uploadedProfile;
    private final Timeline refresh;
    EngineControlPane(Stage owner,ConfigurationPane.Access access,Consumer<EngineOutputClient.State> onState){
        this.access=access;this.onState=onState;setSpacing(12);setPadding(new javafx.geometry.Insets(0,24,24,24));
        setId("engine-control-pane");read.setId("engine-outputs-read");arm.setId("engine-arm");
        stop.setId("engine-stop");configure.setId("engine-configure");stop.getStyleClass().add("danger");
        load.setId("engine-load");autonomous.setId("engine-autonomous");
        tests=new OutputTestPane(owner,operation->run(operation));
        result.setId("engine-output-state");result.setWrapText(true);
        for(int i=1;i<=12;++i)count.getItems().add(i);
        count.setPromptText("Seleciona cilindros");count.setId("engine-cylinders");
        Map<String,GridPane> grids=new LinkedHashMap<>();
        for(String group:java.util.List.of("Motor","Injeção","Ignição","Sincronização","Auxiliares")){
            GridPane grid=new GridPane();grid.setHgap(18);grid.setVgap(10);grids.put(group,grid);
        }
        grids.get("Motor").addRow(0,new Label("Cilindros"),count);
        presets.getItems().addAll(io.tunuino.calibration.EnginePreset.choices());presets.getSelectionModel().select(3);
        presets.setId("engine-preset");applyPreset.setId("engine-preset-apply");
        applyPreset.setOnAction(event->applyPreset());
        String[][] definitions={
            {"fuel","ReqFuel de referência (ms)",""},{"dwell","Dwell (ms, máximo 5)",""},
            {"dead","Tempo morto injetor (ms)",""},{"maximum","Pulso máximo injetor (ms, até 20)",""},
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
        for(String[] d:definitions){
            String group=switch(d[0]){case "fuel","dead","maximum","injectionEnd"->"Injeção";
                case "dwell"->"Ignição";case "toothZero","camFirst","camLast","camPhase"->"Sincronização";
                default->"Auxiliares";};
            GridPane grid=grids.get(group);int row=grid.getRowCount();
            ComboBox<String> choices=new ComboBox<>();choices.setEditable(true);choices.setPrefWidth(190);
            switch(d[0]){
                case "dwell"->choices.getItems().addAll("1","1.5","2","2.5","3","4","5");
                case "dead"->choices.getItems().addAll("0","0.5","0.8","1","1.2","1.5");
                case "maximum"->choices.getItems().addAll("10","15","20");
                case "prime"->choices.getItems().addAll("0","500","1000","1500","2000","3000");
                case "fanOn"->choices.getItems().addAll("90","95","100","105");
                case "fanOff"->choices.getItems().addAll("85","90","95","100");
                case "camPhase"->choices.getItems().addAll("0","1");
                case "pump","fan","relay"->{for(int i=0;i<=16;i++)choices.getItems().add(Integer.toString(i));}
                default->{}
            }
            selectors.put(d[0],choices);TextField field=choices.getEditor();choices.setValue(d[2]);field.setPrefColumnCount(10);
            field.setId("engine-field-"+d[0]);fields.put(d[0],field);
            grid.add(new Label(d[1]),0,row);grid.add(choices,1,row);
        }
        wiring.setHgap(12);wiring.setVgap(7);
        wiring.addRow(0,new Label("Cilindro"),new Label("Injetor LS 1–16"),new Label("Ignição 1–12"),new Label("PMS no ciclo (°)"));
        for(int i=0;i<12;++i){wiring.add(new Label(Integer.toString(i+1)),0,i+1);
            for(int j=0;j<3;++j){TextField f;javafx.scene.Node control;
                if(j<2){ComboBox<String> choice=new ComboBox<>();choice.setEditable(true);choice.setPrefWidth(105);
                    for(int channel=1;channel<=(j==0?16:12);channel++)choice.getItems().add(Integer.toString(channel));
                    cylinderSelectors.put(i+"-"+j,choice);f=choice.getEditor();control=choice;
                }else{f=new TextField();control=f;}
                f.setId("engine-cylinder-"+i+"-"+j);f.setPrefColumnCount(7);cylinders[i][j]=f;wiring.add(control,j+1,i+1);}}
        count.setOnAction(event->{acknowledge.setSelected(false);presetSummary.setText(count.getValue()==null?"Seleciona o número de cilindros ou aplica um modelo.":"Cilindros: "+count.getValue()+" · verifica a ordem e as atribuições no separador Cilindros.");for(int i=0;i<12;++i)for(var f:cylinders[i])f.setDisable(count.getValue()==null||i>=count.getValue());controls();});
        count.getOnAction().handle(null);
        getChildren().add(ConsoleShell.card("PERFIL E CONTROLO DO MOTOR",
            "Perfil sequencial de quatro tempos guardado na ECU. Em modo autónomo, o motor funciona sem USB. Os testes individuais estão no menu Testes de atuadores.",
            ConsoleShell.actions(load,read,stop),result));
        TabPane tabs=new TabPane();tabs.setId("engine-configuration-tabs");tabs.setTabClosingPolicy(TabPane.TabClosingPolicy.UNAVAILABLE);
        tabs.getTabs().add(new Tab("Motor",ConsoleShell.card("MODELO DE CONFIGURAÇÃO",
            "Escolhe cilindros e ordem de ignição. O modelo propõe LS/IGN por número de cilindro e PMS relativos ao cilindro 1. Confirma as ligações e completa a calibração real.",
            ConsoleShell.actions(presets,applyPreset),grids.get("Motor"),presetSummary)));
        presetSummary.setWrapText(true);
        TextField displacement=new TextField("2000"),injectorFlow=new TextField("250");
        displacement.setPrefColumnCount(7);injectorFlow.setPrefColumnCount(7);
        displacement.setId("base-displacement");injectorFlow.setId("base-injector-flow");
        Button generic=new Button("Preencher configuração base");generic.setId("engine-generic-base");
        generic.disableProperty().bind(applyPreset.disableProperty());
        displacement.disableProperty().bind(applyPreset.disableProperty());injectorFlow.disableProperty().bind(applyPreset.disableProperty());
        generic.setOnAction(event->{try{
            var selected=presets.getValue();if(selected==null)return;
            int fuel=io.tunuino.calibration.BaseMaps.gasolineRequiredFuelUs(
                Double.parseDouble(displacement.getText().trim().replace(',','.')),selected.cylinders(),
                Double.parseDouble(injectorFlow.getText().trim().replace(',','.')));
            applyPreset();
            setting("fuel",BigDecimal.valueOf(fuel,3).toPlainString());setting("dwell","3");setting("dead","1");
            setting("maximum","20");setting("prime","2000");setting("fanOn","95");setting("fanOff","90");
            setting("injectionEnd","0");
            autonomous.setSelected(false);
            result.setText("Base local preenchida. ReqFuel estimado para gasolina AFR 14,7, ar 1,184 g/L e combustível 0,745 g/mL. Confirma caudal à pressão usada, dwell e tempo morto. Mede dente zero e janela de fase antes de guardar. Os mapas base estão em Afinação.");
        }catch(IllegalArgumentException error){result.setText("Base: "+error.getMessage());}});
        VBox motorContent=(VBox)tabs.getTabs().get(0).getContent();
        motorContent.getChildren().addAll(ConsoleShell.actions(new Label("Cilindrada (cm³)"),displacement,new Label("Injetor (cm³/min)"),injectorFlow),generic,
            new Label("Exemplo inicial: 2000 cm³ / 250 cm³/min. Substitui pelos dados reais do motor."));
        tabs.getTabs().add(new Tab("Cilindros",ConsoleShell.card("ATRIBUIÇÕES POR CILINDRO",
            "Seleciona a saída de injetor, a saída lógica de ignição e o PMS relativo ao cilindro 1. Ajusta a referência absoluta em Sincronização.",wiring)));
        for(String group:java.util.List.of("Injeção","Ignição","Sincronização","Auxiliares"))
            tabs.getTabs().add(new Tab(group,ConsoleShell.card(group.toUpperCase(java.util.Locale.ROOT),
                group.equals("Sincronização")?"PMS, dente zero e janela de fase dependem do motor. Configura a roda fónica em Sensores e entradas."
                :group.equals("Auxiliares")?"Atribui LS independentes aos relés. 0 significa desativado. Os valores propostos devem corresponder à instalação."
                :"Escolhe um valor da lista ou escreve a calibração da carga instalada. ReqFuel usa VE 100% e MAP 100 kPa, antes do tempo morto.",grids.get(group))));
        getChildren().addAll(tabs,ConsoleShell.card("GUARDAR E ATIVAR", "Guardar não ativa as saídas nesta sessão. O modo autónomo ativa o perfil no próximo arranque elétrico e mantém o motor independente da USB. PARAR mantém as saídas desligadas até nova ativação explícita ou reinício da ECU.",
            autonomous,ConsoleShell.actions(configure),acknowledge,arm));
        read.setOnAction(event->run((session,device)->EngineOutputClient.read(session)));
        load.setOnAction(event->loadProfile());
        configure.setOnAction(event->{try{
            byte[] bytes=profile();
            boolean automatic=autonomous.isSelected();
            if(automatic){
                Alert confirm=ConsoleBranding.alert(Alert.AlertType.CONFIRMATION,
                    "Ao voltar a alimentar a ECU, este perfil poderá ligar bomba, relé e ventoinha e comandar injeção/ignição com sincronismo válido, mesmo sem computador. Confirmas a calibração e as ligações?",ButtonType.OK,ButtonType.CANCEL);
                confirm.initOwner(owner);confirm.setHeaderText("Guardar funcionamento autónomo");
                if(confirm.showAndWait().orElse(ButtonType.CANCEL)!=ButtonType.OK)return;
            }
            run((session,device)->{
                var current=ConfigurationClient.read(session,device,java.time.Duration.ofSeconds(10));
                ConfigurationClient.apply(session,device,current,current.configuration().withEngine(bytes,automatic),java.time.Duration.ofSeconds(25));
                var saved=EngineOutputClient.read(session);
                if(!saved.persisted()||saved.autonomous()!=automatic)throw new IllegalStateException("Estado do motor não confirmou a configuração guardada; lê novamente");
                return saved;
            },bytes);
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
        refresh=new Timeline(new KeyFrame(Duration.millis(750),event->{if(available&&!busy&&state!=null&&state.armed())read.fire();}));
        refresh.setCycleCount(Timeline.INDEFINITE);refresh.play();controls();
    }
    javafx.scene.Node tests(){return tests;}
    void setting(String name,String value){selectors.get(name).setValue(value);fields.get(name).setText(value);}
    private void applyPreset(){
        var preset=presets.getValue();if(preset==null)return;
        count.setValue(preset.cylinders());
        for(int i=0;i<12;i++)for(int j=0;j<3;j++){if(j<2)cylinderSelectors.get(i+"-"+j).setValue("");cylinders[i][j].clear();}
        for(int i=0;i<preset.cylinders();i++){
            cylinderSelectors.get(i+"-0").setValue(Integer.toString(i+1));cylinderSelectors.get(i+"-1").setValue(Integer.toString(i+1));
            cylinders[i][2].setText(BigDecimal.valueOf(preset.tdcCentidegrees(i+1),2).stripTrailingZeros().toPlainString());
        }
        setting("pump",Integer.toString(preset.cylinders()+1));
        setting("fan",Integer.toString(preset.cylinders()+2));
        setting("relay",Integer.toString(preset.cylinders()+3));
        presetSummary.setText(preset.name()+" · injetores LS 1–"+preset.cylinders()+" · ignição IGN 1–"+preset.cylinders()
            +" · bomba LS "+(preset.cylinders()+1)+" · ventoinha LS "+(preset.cylinders()+2)+" · relé LS "+(preset.cylinders()+3));
        acknowledge.setSelected(false);
        result.setText("Modelo aplicado apenas à edição. Completa ReqFuel, dwell, tempo morto e sincronização; confirma LS/IGN antes de enviar à ECU.");controls();
    }
    private static int number(String text,int decimals,int min,int max,String name){
        try {if(text.length()>32||!text.trim().matches("[+-]?[0-9]+(?:[.,][0-9]+)?"))throw new ArithmeticException();
            int value=new BigDecimal(text.trim().replace(',','.')).movePointRight(decimals).intValueExact();
            if(value<min||value>max)throw new ArithmeticException();return value;
        }catch(RuntimeException error){throw new IllegalArgumentException(name+": preenche um valor entre "+BigDecimal.valueOf(min,decimals)+" e "+BigDecimal.valueOf(max,decimals));}
    }
    private int n(String name,int decimals,int min,int max){return number(fields.get(name).getText(),decimals,min,max,name);}
    byte[] profile(){
        if(count.getValue()==null)throw new IllegalArgumentException("Seleciona o número de cilindros");
        ByteBuffer p=ByteBuffer.allocate(128);p.put((byte)1).put((byte)(int)count.getValue());
        p.putShort((short)n("fuel",3,100,30000)).putShort((short)n("dwell",3,100,5000));
        p.putShort((short)n("dead",3,0,5000)).putShort((short)n("maximum",3,100,20000));
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
        if(n("fanOff",2,0,12000)>=n("fanOn",2,0,12000)||n("dead",3,0,5000)>=n("maximum",3,100,20000)
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
                +" · injeção "+String.format(java.util.Locale.forLanguageTag("pt-PT"),"%.3f",value.injectionUs()/1000.0)+" ms · perfil "+(value.configured()?value.cylinders()+" cilindros":"por configurar")
                +" · "+(value.persisted()?"guardado na flash":"não guardado")+" · "+(value.autonomous()?"autónomo, independente da USB":"ativação manual com supervisão USB"));
            onState.accept(value);controls();
        },error->{busy=false;state=null;result.setText("Saídas: "+error.getMessage()+". Estado físico por confirmar.");controls();});
    }
    private void loadProfile(){
        if(!available||busy)return;busy=true;controls();
        record Loaded(ConfigurationClient.Snapshot snapshot,EngineOutputClient.State state){}
        access.run((session,device)->new Loaded(ConfigurationClient.read(session,device,java.time.Duration.ofSeconds(10)),EngineOutputClient.read(session)),value->{
            busy=false;state=value.state();var configuration=value.snapshot().configuration();
            if(value.snapshot().validation().dirty()){
                result.setText("Há alterações pendentes na ECU. Guarda ou descarta antes de carregar o perfil ativo.");controls();return;
            }
            byte[] p=configuration.engineProfile();
            autonomous.setSelected(configuration.autonomous());
            if(!io.tunuino.calibration.EngineProfile.valid(p,configuration.bytes()[3]&255,configuration.bytes()[4]&255)){
                uploadedProfile=null;result.setText("Ainda não existe um perfil de motor guardado. Escolhe um modelo e completa a calibração.");
            }else{
                ByteBuffer b=ByteBuffer.wrap(p);count.setValue((int)p[1]);
                String[] keys={"fuel","dwell","dead","maximum","prime","fanOn","fanOff","injectionEnd"};
                for(int i=0;i<keys.length;i++){
                    int raw=i>=5?b.getShort(2+2*i):Short.toUnsignedInt(b.getShort(2+2*i));
                    setting(keys[i],BigDecimal.valueOf(raw,i>=5?2:i<4?3:0).stripTrailingZeros().toPlainString());
                }
                setting("toothZero",BigDecimal.valueOf(b.getInt(18),2).stripTrailingZeros().toPlainString());
                String[] byteKeys={"pump","fan","relay","camFirst","camLast","camPhase"};
                for(int i=0;i<byteKeys.length;i++)setting(byteKeys[i],Integer.toString(p[22+i]&255));
                for(int i=0;i<12;i++){
                    for(int j=0;j<2;j++)cylinderSelectors.get(i+"-"+j).setValue(i<p[1]?Integer.toString((p[32+8*i+j]&255)+1):"");
                    cylinders[i][2].setText(i<p[1]?BigDecimal.valueOf(b.getInt(34+8*i),2).stripTrailingZeros().toPlainString():"");
                }
                uploadedProfile=p;acknowledge.setSelected(false);
                result.setText("Perfil lido da ECU · "+state.description()+" · "+(state.autonomous()?"autónomo, independente da USB":"ativação manual"));
            }
            onState.accept(state);controls();
        },error->{busy=false;result.setText("Leitura do motor: "+error.getMessage());controls();});
    }
    void availability(DeviceIdentity device,boolean connected,boolean blocked){
        if(!java.util.Objects.equals(identity,device)||!connected){identity=device;state=null;uploadedProfile=null;acknowledge.setSelected(false);}
        available=connected&&device!=null&&!blocked;controls();
    }
    private void controls(){boolean idle=available&&!busy;
        boolean armed=state!=null&&state.armed();
        read.setDisable(!idle);load.setDisable(!idle);stop.setDisable(!idle);configure.setDisable(!idle||armed);
        autonomous.setDisable(armed||busy);
        arm.setDisable(!idle||armed||state==null||!state.configured()||!state.timerReady()||!acknowledge.isSelected()||!matchesProfile());
        tests.update(available,busy,state,uploadedProfile,result.getText());
        presets.setDisable(armed||busy);applyPreset.setDisable(armed||busy);
        selectors.values().forEach(combo->combo.setDisable(armed||busy));
        for(var node:wiring.getChildren()){
            Integer row=GridPane.getRowIndex(node);boolean visible=row==null||row==0||count.getValue()!=null&&row<=count.getValue();
            node.setVisible(visible);node.setManaged(visible);
        }
        count.setDisable(armed||busy);
        for(var field:fields.values())field.setDisable(armed||busy);
        for(int i=0;i<12;++i){
            boolean disabled=armed||busy||count.getValue()==null||i>=count.getValue();
            for(var field:cylinders[i])field.setDisable(disabled);
            for(int j=0;j<2;j++)cylinderSelectors.get(i+"-"+j).setDisable(disabled);
        }
    }
    private boolean matchesProfile(){try{return uploadedProfile!=null&&java.util.Arrays.equals(uploadedProfile,profile());}catch(IllegalArgumentException error){return false;}}
    void close(){refresh.stop();}
}
