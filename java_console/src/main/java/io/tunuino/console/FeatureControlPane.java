/* Copyright (c) 2026 Tunuino. Optional fuel strategies and engine limits. */
package io.tunuino.console;
import io.tunuino.calibration.*;
import io.tunuino.protocol.*;
import java.util.*;
import java.util.function.Consumer;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import javafx.stage.*;
final class FeatureControlPane extends VBox {
    private final ConfigurationPane.Access access;private final Consumer<EngineOutputClient.State> onState;
    private final EnumMap<FeatureConfiguration.Field,ComboBox<String>> fields=new EnumMap<>(FeatureConfiguration.Field.class);
    private final EnumMap<FeatureConfiguration.Option,CheckBox> options=new EnumMap<>(FeatureConfiguration.Option.class);
    private final Button read=new Button("Ler configuração e estado"),save=new Button("Guardar funções na ECU"),base=new Button("Aplicar referência desativada"),backup=new Button("Guardar cópia .tnaux"),open=new Button("Abrir cópia .tnaux");
    private final ComboBox<String> reference=new ComboBox<>();
    private final Map<String,GridPane> sensorGrids=new LinkedHashMap<>();
    private final Label result=new Label("Sem configuração recebida da ECU.");
    private Consumer<AuxControlClient.Snapshot> onSnapshot=v->{};
    private AuxControlClient.Snapshot snapshot;private byte[] baseline=AuxConfiguration.reference().bytes();
    private DeviceIdentity identity;private boolean available,busy,armed;
    FeatureControlPane(Stage owner,ConfigurationPane.Access access,Consumer<EngineOutputClient.State> onState){
        this.access=access;this.onState=onState;setSpacing(14);setPadding(new javafx.geometry.Insets(0,24,24,24));setId("feature-control-pane");
        read.setId("features-read");save.setId("features-save");base.setId("features-reference");result.setId("features-state");result.setWrapText(true);
        reference.getItems().addAll("Gasolina · aspirado","Gasolina · sobrealimentado");reference.getSelectionModel().select(0);
        getChildren().add(ConsoleShell.card("COMBUSTÍVEL E PROTEÇÕES","Seleciona as funções necessárias e calibra as entradas. Os valores de referência começam com todas as novas funções desativadas. Guardar exige saídas desarmadas.",ConsoleShell.actions(read,save),result));
        var tabs=new TabPane();tabs.setId("features-tabs");tabs.setTabClosingPolicy(TabPane.TabClosingPolicy.UNAVAILABLE);
        Map<String,GridPane> grids=new LinkedHashMap<>();
        Map<String,String> help=Map.of(
            "Sensores","IAT usa uma entrada TEMP independente do líquido de refrigeração. A tensão usa uma entrada AV e deve ser calibrada em VOLTS DA BATERIA, incluindo o divisor. Mede os pontos ADC; as referências não são calibrações do sensor.",
            "Tensão","Curvas de três tensões para o dead time dos injetores e dwell das bobinas. Todos os tempos são apresentados em milissegundos. Usa os dados do fabricante dos componentes instalados.",
            "Lambda","Curva alvo de três pontos MAP absolutos. O combustível base deve referir-se a lambda 1; ativar o alvo enriquece por 1 / lambda alvo. A malha fechada é PI com correção limitada e condições de temperatura, RPM, TPS e atraso. Não altera o mapa VE nem faz Autotune.",
            "Transientes","Aceleração: combustível adicional por velocidade de abertura TPS, com decaimento linear. DFCO: corte com temperatura mínima, atraso e histerese de RPM. A malha fechada suspende a correção durante estes eventos.",
            "Limites","Limitador e launch cortam combustível com histerese; a ignição mantém o dwell programado. Launch exige uma entrada AV condicionada, com níveis válidos de 100 a 3995 ADC. Um sinal fora dessa faixa causa corte retido. Proteções CLT/tensão também exigem reativação explícita.",
            "Flex fuel","Requer controlador de etanol com saída analógica calibrada entre 0 e 100%. Esta entrada não aceita diretamente sensores de frequência. O firmware interpola a correção de combustível e avanço; afina para o combustível e motor usados.",
            "Cilindros","Ajustes individuais sobre o combustível efetivo, antes do dead time, e sobre o avanço. Confirmar medição por cilindro antes de ativar. Os limites de pulso e duty continuam a aplicar-se.");
        for(String group:List.of("Sensores","Tensão","Lambda","Transientes","Limites","Flex fuel","Cilindros")){
            GridPane grid=new GridPane();grid.setHgap(18);grid.setVgap(9);grids.put(group,grid);
            var column=new ColumnConstraints();column.setPercentWidth(50);grid.getColumnConstraints().addAll(column,new ColumnConstraints());
            var scroll=new ScrollPane(ConsoleShell.card(group.toUpperCase(Locale.ROOT),help.get(group),grid));scroll.setFitToWidth(true);scroll.setPrefViewportHeight(560);tabs.getTabs().add(new Tab(group.equals("Sensores")?"Compensações":group,scroll));
        }
        for(var option:FeatureConfiguration.Option.values()){var c=new CheckBox(option.label);c.setId("feature-option-"+option.name());c.setWrapText(true);options.put(option,c);GridPane g=grids.get(option.group);g.add(c,0,g.getRowCount(),2,1);}
        sensorGrids.put("IAT",new GridPane());sensorGrids.put("VOLT",new GridPane());
        sensorGrids.values().forEach(g->{g.setHgap(16);g.setVgap(9);});
        grids.get("Sensores").addRow(grids.get("Sensores").getRowCount(),new Label("Calibra IAT e tensão em Sensores e calibrações."));
        for(var f:FeatureConfiguration.Field.values()){
            var c=new ComboBox<String>();c.setEditable(true);c.setPrefWidth(165);c.setId("feature-choice-"+f.name());c.getEditor().setId("feature-field-"+f.name());
            if(f.offset<76){for(int i=f.min;i<=f.max;i++)c.getItems().add(Integer.toString(i));
                String function=switch(f){case IAT_INPUT->"IAT — temperatura do ar";case VOLT_INPUT->"Tensão da bateria";case LAUNCH_INPUT->"Comando de launch control";case FLEX_INPUT->"Etanol — flex fuel";default->throw new IllegalArgumentException();};
                FunctionChoices.numericStrings(c,f==FeatureConfiguration.Field.IAT_INPUT?io.tunuino.calibration.IoAssignments.Group.TEMPERATURE:io.tunuino.calibration.IoAssignments.Group.ANALOG,switch(f){case IAT_INPUT->2;case VOLT_INPUT->4;case LAUNCH_INPUT->6;case FLEX_INPUT->5;default->throw new IllegalArgumentException();});
            }
            else if(f.decimals==3&&f.name().contains("TARGET"))c.getItems().addAll("0.85","0.9","0.95","1");
            else c.getItems().add(java.math.BigDecimal.valueOf(f.reference,f.decimals).stripTrailingZeros().toPlainString());
            fields.put(f,c);
            if(f.name().startsWith("DEAD")||f.name().startsWith("VOLT_AXIS"))continue;
            GridPane g=f.group.equals("Sensores")?sensorGrids.get(f.name().startsWith("IAT_")?"IAT":"VOLT"):grids.get(f.group);var l=new Label(f.label);l.setWrapText(true);g.addRow(g.getRowCount(),l,f.offset<76?FunctionChoices.control(c):c);
        }
        GridPane voltage=grids.get("Tensão");voltage.add(deadTimeTable(),0,voltage.getRowCount(),2,1);
        getChildren().addAll(tabs,ConsoleShell.card("REFERÊNCIAS E CÓPIAS","O ficheiro .tnaux inclui TPS, ralenti, turbo e todas estas funções. Abrir uma cópia prepara a edição; só Guardar envia à ECU. Guarda também o perfil .tncfg e os mapas .tnmap.",ConsoleShell.actions(reference,base,backup,open)));
        fill(baseline);
        base.setOnAction(e->{byte[] p=FeatureConfiguration.reference(baseline).bytes();if(reference.getSelectionModel().getSelectedIndex()==1){var b=java.nio.ByteBuffer.wrap(p);b.putShort(118,(short)25000).putShort(124,(short)850);}fill(p);result.setText("Referência aplicada à edição, com todas as novas funções desativadas. As definições TPS/ralenti/turbo foram preservadas.");});
        read.setOnAction(e->run((s,d)->new Loaded(AuxControlClient.read(s,d,java.time.Duration.ofSeconds(10)),AuxControlClient.features(s),EngineOutputClient.read(s))));
        save.setOnAction(e->{try{var candidate=candidate();if(snapshot==null)throw new IllegalArgumentException("Lê a configuração antes de guardar");
            var a=ConsoleBranding.alert(Alert.AlertType.CONFIRMATION,"Guardar as funções selecionadas para a próxima ativação do motor? Confirma sensores, curvas e limites para o motor e componentes instalados.",ButtonType.OK,ButtonType.CANCEL);a.initOwner(owner);a.setHeaderText("Guardar combustível e proteções");if(a.showAndWait().orElse(ButtonType.CANCEL)!=ButtonType.OK)return;
            var expected=snapshot;run((s,d)->new Loaded(AuxControlClient.apply(s,d,expected,candidate,java.time.Duration.ofSeconds(25)),AuxControlClient.features(s),EngineOutputClient.read(s)));
        }catch(IllegalArgumentException x){result.setText(x.getMessage());}});
        backup.setOnAction(e->{try{var c=candidate();var f=chooser("Guardar auxiliares e funções");f.setInitialFileName("auxiliares-funcoes.tnaux");var file=f.showSaveDialog(owner);if(file!=null){c.writeNew(file.toPath());result.setText("Cópia guardada: "+file.getName());}}catch(Exception x){result.setText(x.getMessage());}});
        open.setOnAction(e->{var file=chooser("Abrir auxiliares e funções").showOpenDialog(owner);if(file!=null)try{fill(AuxConfiguration.read(file.toPath()).bytes());result.setText("Cópia aberta apenas na edição.");}catch(Exception x){result.setText(x.getMessage());}});controls();
    }
    javafx.scene.Node sensorCalibration(String kind){
        var grid=sensorGrids.get(kind);Button loadSensor=new Button("Ler calibração da ECU"),saveSensor=new Button("Guardar calibração "+(kind.equals("IAT")?"IAT":"da tensão"));
        loadSensor.disableProperty().bind(read.disabledProperty());saveSensor.disableProperty().bind(save.disabledProperty());
        loadSensor.setOnAction(e->read.fire());saveSensor.setId("sensor-"+kind.toLowerCase(Locale.ROOT)+"-save");
        saveSensor.setOnAction(e->{try{
            var expected=snapshot;if(expected==null||!available||busy||armed)return;
            var values=new EnumMap<FeatureConfiguration.Field,String>(FeatureConfiguration.Field.class);
            fields.forEach((f,c)->{if(f.group.equals("Sensores")&&f.name().startsWith(kind+"_"))values.put(f,c.getEditor().getText());});
            var candidate=SensorCalibration.sensor(new AuxConfiguration(baseline),kind,values);
            busy=true;controls();access.run((session,device)->new Loaded(AuxControlClient.apply(session,device,expected,candidate,java.time.Duration.ofSeconds(25)),AuxControlClient.features(session),EngineOutputClient.read(session)),v->{
                busy=false;snapshot=v.snapshot();baseline=v.snapshot().configuration().bytes();armed=v.engine().armed();
                result.setText("Calibração guardada e confirmada por leitura. Modos e compensações mantêm o estado anterior.");onSnapshot.accept(v.snapshot());onState.accept(v.engine());controls();
            },x->{busy=false;result.setText(x.getMessage());controls();});
        }catch(IllegalArgumentException x){result.setText(x.getMessage());}});
        VBox box=new VBox(10,ConsoleShell.actions(loadSensor,saveSensor));
        boolean air=kind.equals("IAT");
        var rawFields=air?List.of(FeatureConfiguration.Field.IAT_RAW1,FeatureConfiguration.Field.IAT_RAW2,FeatureConfiguration.Field.IAT_RAW3)
            :List.of(FeatureConfiguration.Field.VOLT_RAW_LOW,FeatureConfiguration.Field.VOLT_RAW_HIGH);
        var physicalFields=air?List.of(FeatureConfiguration.Field.IAT_TEMP1,FeatureConfiguration.Field.IAT_TEMP2,FeatureConfiguration.Field.IAT_TEMP3)
            :List.of(FeatureConfiguration.Field.VOLT_LOW,FeatureConfiguration.Field.VOLT_HIGH);
        box.getChildren().add(new MeasuredCalibrationPane("sensor-"+kind.toLowerCase(Locale.ROOT)+"-auto",air?"IAT":"Tensão da bateria",
            air?"Mede três temperaturas conhecidas com um termómetro. Os valores iniciais são referências editáveis; a Console captura o ADC real."
                :"Mede duas tensões distintas com multímetro, em volts da bateria. As referências são editáveis; o divisor da entrada faz parte da calibração.",
            air?new String[]{"Estabiliza na primeira temperatura conhecida (°C).","Estabiliza na segunda temperatura conhecida (°C).","Estabiliza na terceira temperatura conhecida (°C)."}
                :new String[]{"Estabiliza na primeira tensão medida com multímetro (V).","Estabiliza na segunda tensão medida com multímetro (V)."},
            air?new double[]{20,60,100}:new double[]{12,14},air?-40:0,air?150:24,air,false,access,
            ()->Integer.parseInt(fields.get(air?FeatureConfiguration.Field.IAT_INPUT:FeatureConfiguration.Field.VOLT_INPUT).getEditor().getText()),
            ()->Arrays.asList(identity,fields.get(air?FeatureConfiguration.Field.IAT_INPUT:FeatureConfiguration.Field.VOLT_INPUT).getEditor().getText(),snapshot),
            save.disabledProperty(),points->{for(int i=0;i<points.length;i++){
                fields.get(rawFields.get(i)).setValue(Integer.toString(points[i].adc()));
                fields.get(physicalFields.get(i)).setValue(Double.toString(points[i].value()));
            }}));
        if(kind.equals("IAT"))box.getChildren().add(SensorCalibrationControls.ntc("iat-ntc",points->{
            var raw=java.util.List.of(FeatureConfiguration.Field.IAT_RAW1,FeatureConfiguration.Field.IAT_RAW2,FeatureConfiguration.Field.IAT_RAW3);
            var temp=java.util.List.of(FeatureConfiguration.Field.IAT_TEMP1,FeatureConfiguration.Field.IAT_TEMP2,FeatureConfiguration.Field.IAT_TEMP3);
            for(int i=0;i<3;i++){fields.get(raw.get(i)).setValue(Integer.toString(points[i].adc()));fields.get(temp.get(i)).setValue(Double.toString(points[i].temperature()));}
        },base.disabledProperty()));
        int row=0;for(var f:FeatureConfiguration.Field.values())if(f.group.equals("Sensores")&&f.name().startsWith(kind+"_")){
            if(f.name().contains("RAW")){
                Button measure=new Button("Medir ponto");measure.setId("sensor-measure-"+f.name());measure.disableProperty().bind(read.disabledProperty().or(base.disabledProperty()));
                measure.setOnAction(e->{int channel=Integer.parseInt(fields.get(kind.equals("IAT")?FeatureConfiguration.Field.IAT_INPUT:FeatureConfiguration.Field.VOLT_INPUT).getValue());if(channel==0){result.setText("Escolhe primeiro a ligação do sensor");return;}
                    access.run((session,device)->io.tunuino.protocol.IoClient.read(session,device,java.time.Duration.ofSeconds(2)),v->{try{fields.get(f).setValue(Integer.toString(SensorCalibration.measured(v,channel,kind.equals("IAT"))));result.setText("Ponto medido na edição. Indica a temperatura ou tensão conhecida e guarda para aplicar.");}catch(IllegalArgumentException x){result.setText(x.getMessage());}},x->result.setText(x.getMessage()));
                });grid.add(measure,2,row);
            }row++;
        }
        Label note=new Label(kind.equals("IAT")?"Curva de três pontos da temperatura do ar. Calibrar não ativa a compensação de combustível.":"Dois pontos medidos com multímetro, em volts da bateria. A entrada AV exige condicionamento/divisor adequado; não ligar 12 V diretamente ao ADC.");note.setWrapText(true);
        ConsoleLanguage.watch(result.textProperty());Label message=new Label();message.textProperty().bind(result.textProperty());message.setWrapText(true);box.getChildren().addAll(note,grid,message);return box;
    }
    private javafx.scene.Node deadTimeTable() {
        var table=new TableView<Integer>();table.setId("injector-dead-time-table");table.setEditable(true);
        table.getItems().addAll(1,2,3);table.setFixedCellSize(34);table.setPrefHeight(140);table.setMaxHeight(140);
        TableColumn<Integer,String> volts=new TableColumn<>("Tensão da bateria (V)"),dead=new TableColumn<>("Dead time (ms)");
        for(var column:List.of(volts,dead)) {
            boolean voltage=column==volts;
            column.setCellValueFactory(v->fields.get(FeatureConfiguration.Field.valueOf((voltage?"VOLT_AXIS":"DEAD")+v.getValue())).getEditor().textProperty());
            column.setCellFactory(javafx.scene.control.cell.TextFieldTableCell.forTableColumn());
            column.setSortable(false);column.setReorderable(false);
            column.setOnEditCommit(e->{
                var field=FeatureConfiguration.Field.valueOf((voltage?"VOLT_AXIS":"DEAD")+e.getRowValue());
                try {
                    var number=new java.math.BigDecimal(e.getNewValue().trim().replace(',','.'));
                    int value=number.movePointRight(field.decimals).intValueExact();
                    if(value<field.min||value>field.max)throw new IllegalArgumentException();
                    setting(field,number.stripTrailingZeros().toPlainString());
                    result.setText("Curva alterada apenas na edição; guarda para aplicar à ECU.");
                }catch(RuntimeException error){result.setText("Valor inválido. Usa volts e milissegundos dentro dos limites da curva.");table.refresh();}
            });
        }
        table.getColumns().addAll(List.of(volts,dead));table.setColumnResizePolicy(TableView.CONSTRAINED_RESIZE_POLICY_FLEX_LAST_COLUMN);
        table.disableProperty().bind(base.disabledProperty());
        Button referenceCurve=new Button("Usar referência inicial · 8 / 12 / 16 V");referenceCurve.setId("injector-dead-time-reference");
        referenceCurve.disableProperty().bind(base.disabledProperty());referenceCurve.setOnAction(e->{
            for(int row=1;row<=3;row++)for(String prefix:List.of("VOLT_AXIS","DEAD")){
                var f=FeatureConfiguration.Field.valueOf(prefix+row);setting(f,java.math.BigDecimal.valueOf(f.reference,f.decimals).stripTrailingZeros().toPlainString());
            }
            result.setText("Referência genérica preparada na edição. Confirma a curva do fabricante para o injetor e pressão diferencial usados.");
        });
        Label note=new Label("Referência inicial: 8 V → 1,500 ms; 12 V → 1,000 ms; 16 V → 0,700 ms. Não identifica um injetor. Usa a curva do fabricante à pressão diferencial instalada. Duplo clique para editar; a mesma coluna de tensão define os três pontos de dwell.");note.setWrapText(true);
        return new VBox(8,new Label("INJECTOR DEAD TIME"),table,referenceCurve,note);
    }
    private void setting(FeatureConfiguration.Field field,String value){fields.get(field).setValue(value);fields.get(field).getEditor().setText(value);}
    private static FileChooser chooser(String title){var f=new FileChooser();f.setTitle(title);f.getExtensionFilters().add(new FileChooser.ExtensionFilter("Tunuino auxiliares e funções","*.tnaux"));return f;}
    private record Loaded(AuxControlClient.Snapshot snapshot,AuxControlClient.FeatureState features,EngineOutputClient.State engine){}
    private void run(ConfigurationPane.Operation<Loaded> operation){if(!available||busy)return;busy=true;controls();access.run(operation,v->{busy=false;snapshot=v.snapshot();armed=v.engine().armed();fill(v.snapshot().configuration().bytes());var f=v.features();result.setText(f.description()+" · "+v.engine().description());onSnapshot.accept(v.snapshot());onState.accept(v.engine());controls();},x->{busy=false;result.setText(x.getMessage());controls();});}
    private void fill(byte[] page){baseline=(page[0]==1?page:AuxConfiguration.reference().bytes()).clone();var f=new FeatureConfiguration(baseline);if(!f.configured())f=FeatureConfiguration.reference(baseline);for(var field:fields.keySet()){String text=f.value(field);fields.get(field).setValue(text);fields.get(field).getEditor().setText(text);}for(var o:options.keySet())options.get(o).setSelected(f.enabled(o));}
    AuxConfiguration candidate(){var values=new EnumMap<FeatureConfiguration.Field,String>(FeatureConfiguration.Field.class);fields.forEach((f,c)->values.put(f,c.getEditor().getText()));var enabled=EnumSet.noneOf(FeatureConfiguration.Option.class);options.forEach((o,c)->{if(c.isSelected())enabled.add(o);});return new AuxConfiguration(new FeatureConfiguration(baseline).edited(values,enabled).bytes());}
    void onSnapshot(Consumer<AuxControlClient.Snapshot> action){onSnapshot=action;}
    void sensorSaved(AuxControlClient.Snapshot value){
        if(snapshot==null)return;
        byte[] expected=snapshot.configuration().bytes(),received=value.configuration().bytes();
        if(java.util.Arrays.equals(java.util.Arrays.copyOfRange(expected,68,256),java.util.Arrays.copyOfRange(received,68,256))){
            snapshot=value;baseline=value.configuration().bytes();
        }else{snapshot=null;result.setText("A calibração desta secção mudou. Lê a ECU antes de guardar; a edição local foi mantida.");}
        controls();
    }
    void coreSaved(io.tunuino.protocol.ConfigurationClient.Validation validation){
        if(snapshot!=null&&!validation.dirty()&&!snapshot.validation().dirty()){
            long next=(snapshot.validation().generation()+1)&0xffffffffL;if(next==0)next=1;
            if(validation.generation()==next)snapshot=new AuxControlClient.Snapshot(snapshot.configuration(),validation,snapshot.pageCrc());
        }controls();
    }
    void invalidateProject(){snapshot=null;result.setText("Projeto restaurado. Lê novamente esta secção antes de guardar; a edição local foi mantida.");controls();}
    void availability(DeviceIdentity d,boolean connected,boolean blocked,boolean engineArmed){if(!Objects.equals(identity,d)||!connected){identity=d;snapshot=null;}available=connected&&d!=null&&!blocked;armed=engineArmed;controls();}
    private void controls(){boolean edit=!busy&&!armed;read.setDisable(!available||busy);save.setDisable(!available||busy||armed||snapshot==null);base.setDisable(!edit);reference.setDisable(!edit);open.setDisable(!edit);backup.setDisable(busy);fields.values().forEach(c->c.setDisable(!edit));options.values().forEach(c->c.setDisable(!edit));}
}
