/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.calibration.IoConfiguration;
import io.tunuino.protocol.ConfigurationClient;
import io.tunuino.protocol.DeviceIdentity;
import io.tunuino.protocol.EnginePreviewClient;
import io.tunuino.protocol.IoClient;
import io.tunuino.transport.PacketSession;
import java.io.File;
import java.time.Duration;
import java.util.Arrays;
import java.util.EnumMap;
import java.util.List;
import java.util.Locale;
import java.util.Objects;
import java.util.function.Consumer;
import javafx.beans.property.SimpleStringProperty;
import javafx.geometry.Insets;
import javafx.scene.control.Alert;
import javafx.scene.control.Button;
import javafx.scene.control.ButtonType;
import javafx.scene.control.CheckBox;
import javafx.scene.control.ComboBox;
import javafx.scene.control.Label;
import javafx.scene.control.TableColumn;
import javafx.scene.control.TableView;
import javafx.scene.control.Tab;
import javafx.scene.control.TabPane;
import javafx.scene.control.TextField;
import javafx.scene.layout.GridPane;
import javafx.scene.layout.TilePane;
import javafx.scene.layout.VBox;
import javafx.stage.FileChooser;
import javafx.stage.Stage;

/** Actual IO settings editor; all transport work is delegated to the exclusive USB worker. */
public final class ConfigurationPane extends VBox {
    @FunctionalInterface public interface Operation<T> { T run(PacketSession session,DeviceIdentity identity)throws Exception; }
    public interface Access { <T> void run(Operation<T> operation,Consumer<T> success,Consumer<Throwable> failure); }
    public record AdcRow(String channel,String raw,String volts){}
    private final Stage owner;
    private final Access access;
    private final EnumMap<IoConfiguration.Field,TextField> fields=new EnumMap<>(IoConfiguration.Field.class);
    private final CheckBox trigger=new CheckBox("Ativar captura de cambota/cames");
    private final ComboBox<Integer> map=channels(io.tunuino.calibration.IoAssignments.Group.ANALOG,1),lambda=channels(io.tunuino.calibration.IoAssignments.Group.ANALOG,2),coolant=channels(io.tunuino.calibration.IoAssignments.Group.TEMPERATURE,1);
    private final Button read=new Button("Ler configuração"),apply=new Button("Guardar na ECU"),
        discard=new Button("Descartar pendentes da ECU"),reset=new Button("Repor edição"),
        newLocal=new Button("Nova configuração local"),saveFile=new Button("Guardar ficheiro"),openFile=new Button("Abrir ficheiro"),readInputs=new Button("Ler entradas ADC");
    private final Label feedback=new Label("Liga e identifica a ECU para ler a configuração."),
        validation=new Label("Sem validação recebida do firmware."),inputState=new Label("Sem leitura das entradas.");
    private final TableView<AdcRow> adc=new TableView<>();
    private final TabPane forms=new TabPane();
    private final java.util.List<Button> pointButtons=new java.util.ArrayList<>();
    private Consumer<ConfigurationClient.Validation> onSaved=v->{};
    private ConfigurationClient.Snapshot snapshot;
    private IoConfiguration base=IoConfiguration.localDefaults(),baseline=base;
    private DeviceIdentity device;
    private DeviceIdentity loadedDevice;
    private boolean supported,busy,loading;

    public ConfigurationPane(Stage owner,Access access) {
        this.owner=owner;this.access=access;setId("sensor-configuration-pane");setSpacing(16);setPadding(new Insets(0,24,24,24));
        read.setId("config-read");apply.setId("config-apply");discard.setId("config-discard");
        feedback.setId("config-feedback");validation.setId("config-validation");readInputs.setId("config-read-adc");
        trigger.setId("config-trigger");map.setId("config-map-channel");lambda.setId("config-lambda-channel");coolant.setId("config-coolant-channel");
        apply.getStyleClass().add("primary");read.getStyleClass().add("primary");
        for(Label label:List.of(feedback,validation,inputState))label.setWrapText(true);
        getChildren().add(ConsoleShell.card("CONFIGURAÇÃO DA CENTRALINA",
            "Prepara a configuração offline ou lê a ECU. Guardar na ECU exige uma leitura desta centralina e confirma a persistência; os mapas são preservados.",
            ConsoleShell.actions(read,apply),feedback,validation,
            editorActions(newLocal,reset,openFile,saveFile,discard)));
        forms.setId("config-form-tabs");forms.setTabClosingPolicy(TabPane.TabClosingPolicy.UNAVAILABLE);
        for(String group:List.of("Aquisição","MAP","Lambda","Temperatura")) {
            GridPane grid=new GridPane();grid.setHgap(16);grid.setVgap(10);int row=0;
            if(group.equals("Aquisição"))grid.add(trigger,0,row++,2,1);
            else {ComboBox<Integer> selector=group.equals("MAP")?map:group.equals("Lambda")?lambda:coolant;
                grid.add(new Label("Ligação do sensor"),0,row);grid.add(FunctionChoices.control(selector),1,row++);}
            for(IoConfiguration.Field field:IoConfiguration.Field.values())if(field.group.equals(group)) {
                TextField editor=new TextField();editor.setPrefColumnCount(10);editor.setMaxWidth(160);
                editor.setId("config-field-"+field.name());fields.put(field,editor);
                grid.add(new Label(field.label),0,row);grid.add(editor,1,row);
                if(field.name().contains("RAW")) {
                    Button measure=new Button("Medir ponto");measure.setId("config-measure-"+field.name());pointButtons.add(measure);
                    measure.setOnAction(e->{int ch=group.equals("MAP")?map.getValue():group.equals("Lambda")?lambda.getValue():coolant.getValue();
                        if(ch==0){feedback.setText("Escolhe primeiro a ligação do sensor");return;}
                        access.run((session,device)->IoClient.read(session,device,Duration.ofSeconds(2)),v->{try{editor.setText(Integer.toString(io.tunuino.calibration.SensorCalibration.measured(v,ch,group.equals("Temperatura"))));showInputs(v);feedback.setText("Ponto medido na edição. Indica o valor físico conhecido e guarda para aplicar.");}catch(IllegalArgumentException x){failed(x);}},this::failed);
                    });grid.add(measure,2,row);
                }
                row++;
                editor.textProperty().addListener((p,old,value)->{if(!loading)refresh();});
            }
            String subtitle=group.equals("Aquisição")?"Configuração de aquisição; não ativa injetores, bobinas ou outras saídas."
                :group.equals("Temperatura")?"Três pontos ADC/temperatura. Usa a curva medida do teu sensor."
                :"Dois pontos ADC/valor físico. Usa ponto decimal e a calibração real do sensor.";
            VBox card=ConsoleShell.card(group.toUpperCase(Locale.ROOT),subtitle,grid);
            if(!group.equals("Temperatura"))card.getChildren().add(2,preset(group));
            else card.getChildren().add(2,SensorCalibrationControls.ntc("clt-ntc",p->{
                IoConfiguration.Field[] raw={IoConfiguration.Field.TEMP_RAW_1,IoConfiguration.Field.TEMP_RAW_2,IoConfiguration.Field.TEMP_RAW_3};
                IoConfiguration.Field[] temp={IoConfiguration.Field.TEMP_1,IoConfiguration.Field.TEMP_2,IoConfiguration.Field.TEMP_3};
                for(int i=0;i<3;i++){fields.get(raw[i]).setText(Integer.toString(p[i].adc()));fields.get(temp[i]).setText(Double.toString(p[i].temperature()));}
            },newLocal.disabledProperty()));
            forms.getTabs().add(new Tab(group.equals("Aquisição")?"Roda fónica":group.equals("Temperatura")?"CLT · motor":group,card));
        }
        getChildren().add(forms);
        TableColumn<AdcRow,String> name=new TableColumn<>("Entrada"),raw=new TableColumn<>("ADC"),volts=new TableColumn<>("Tensão de diagnóstico (V)");
        name.setCellValueFactory(v->new SimpleStringProperty(v.getValue().channel()));
        raw.setCellValueFactory(v->new SimpleStringProperty(v.getValue().raw()));
        volts.setCellValueFactory(v->new SimpleStringProperty(v.getValue().volts()));
        adc.getColumns().setAll(List.of(name,raw,volts));adc.setColumnResizePolicy(TableView.CONSTRAINED_RESIZE_POLICY_FLEX_LAST_COLUMN);
        adc.setPrefHeight(360);adc.setId("config-adc-table");
        forms.getTabs().add(new Tab("Entradas ADC",ConsoleShell.card("ENTRADAS E ESTADO", "A tensão mostrada é no pino MCU; os divisores externos não são inferidos.",readInputs,inputState,adc)));
        trigger.selectedProperty().addListener((p,a,b)->refresh());
        for(ComboBox<Integer> selector:List.of(map,lambda,coolant))selector.setOnAction(e->{if(!loading&&!FunctionChoices.updating(selector))refresh();});
        read.setOnAction(e->{
            if(hasLocalChanges()&&!confirm("Repor edição local?","A leitura substituirá a edição local pelos valores da ECU."))return;
            access.run((s,id)->ConfigurationClient.read(s,id,Duration.ofSeconds(5)),this::loaded,this::failed);
        });
        newLocal.setId("config-new-local");newLocal.setOnAction(e->{
            if(hasLocalChanges()&&!confirm("Nova configuração local?","A edição atual será substituída; os dados da ECU não são alterados."))return;
            snapshot=null;loadedDevice=null;base=IoConfiguration.localDefaults();baseline=base;display(base);validation.setText("Edição local · sem validação de ECU associada.");
            feedback.setText("Nova configuração local. As entradas estão desativadas; lê a ECU antes de aplicar valores ao dispositivo.");refresh();
        });
        apply.setOnAction(e->apply());reset.setOnAction(e->{if(baseline!=null){base=baseline;display(base);feedback.setText("Edição local reposta; a ECU não foi alterada.");refresh();}});
        discard.setOnAction(e->{
            if(confirm("Descartar os valores pendentes na ECU?","As alterações pendentes de configuração e mapas serão substituídas pelos valores ativos."))
                access.run((s,id)->ConfigurationClient.discard(s,id,Duration.ofSeconds(5)),this::loaded,this::failed);
        });
        readInputs.setOnAction(e->access.run((s,id)->IoClient.read(s,id,Duration.ofSeconds(2)),this::showInputs,this::failed));
        saveFile.setOnAction(e->file(false));openFile.setOnAction(e->file(true));
        display(base);feedback.setText("Configuração local disponível. Escolhe a roda fónica e as calibrações; os sensores começam desativados.");refresh();
    }
    private javafx.scene.Node preset(String group){
        ComboBox<String> choices=new ComboBox<>();choices.setId("config-preset-"+group);choices.setPrefWidth(310);
        choices.getItems().add("Personalizado");
        if(group.equals("Aquisição"))choices.getItems().addAll("36-1","60-2","24-1","12-1");
        if(group.equals("MAP"))choices.getItems().addAll("Linear ADC 0–4095 → 0–100 kPa","Linear ADC 0–4095 → 0–250 kPa","Linear ADC 0–4095 → 0–400 kPa");
        if(group.equals("Lambda"))choices.getItems().addAll("Linear ADC 0–4095 → λ 0.5–1.5","Linear ADC 0–4095 → λ 0.7–1.3");
        choices.getSelectionModel().selectFirst();
        Button use=new Button("Aplicar à edição");use.setId("config-preset-apply-"+group);
        use.setOnAction(e->{int selected=choices.getSelectionModel().getSelectedIndex();if(selected<=0)return;
            if(group.equals("Aquisição")){
                String[] wheel=choices.getValue().split("-");fields.get(IoConfiguration.Field.WHEEL).setText(wheel[0]);fields.get(IoConfiguration.Field.MISSING).setText(wheel[1]);
            }else if(group.equals("MAP")){
                fields.get(IoConfiguration.Field.MAP_RAW_LOW).setText("0");fields.get(IoConfiguration.Field.MAP_RAW_HIGH).setText("4095");
                fields.get(IoConfiguration.Field.MAP_LOW).setText("0");fields.get(IoConfiguration.Field.MAP_HIGH).setText(selected==1?"100":selected==2?"250":"400");
            }else {
                fields.get(IoConfiguration.Field.LAMBDA_RAW_LOW).setText("0");fields.get(IoConfiguration.Field.LAMBDA_RAW_HIGH).setText("4095");
                fields.get(IoConfiguration.Field.LAMBDA_LOW).setText(selected==1?"0.5":"0.7");fields.get(IoConfiguration.Field.LAMBDA_HIGH).setText(selected==1?"1.5":"1.3");
            }
            feedback.setText(group.equals("Aquisição")?"Roda aplicada à edição; a captura não é ativada automaticamente.":"Curva linear aplicada à edição. Confirma os pontos ADC reais do sensor/controlador e seleciona o canal antes de guardar.");refresh();
        });
        choices.disableProperty().bind(newLocal.disableProperty());use.disableProperty().bind(newLocal.disableProperty());
        return ConsoleShell.actions(choices,use);
    }
    private javafx.scene.Node editorActions(Button... buttons){
        var pane=new javafx.scene.control.TitledPane("Ficheiros e edição",ConsoleShell.actions(buttons));pane.setExpanded(false);pane.setAnimated(false);return pane;
    }
    void onSaved(Consumer<ConfigurationClient.Validation> action){onSaved=action;}
    void auxiliarySaved(io.tunuino.protocol.AuxControlClient.Snapshot value){
        if(snapshot!=null&&!value.validation().dirty()&&snapshot.validation().pageCrc()==value.validation().pageCrc())
            snapshot=new ConfigurationClient.Snapshot(snapshot.configuration(),value.validation());
        refresh();
    }
    void addSensorPage(String name, javafx.scene.Node content){forms.getTabs().add(forms.getTabs().size()-1,new Tab(name,content));}
    private static ComboBox<Integer> channels(io.tunuino.calibration.IoAssignments.Group group,int function) {
        ComboBox<Integer> choice=new ComboBox<>();for(int i=0;i<=group.count;i++)choice.getItems().add(i);choice.setValue(0);FunctionChoices.integers(choice,group,function);return choice;
    }
    public void availability(DeviceIdentity identity,boolean connected,boolean busy) {
        if(!connected||!Objects.equals(device,identity)) {
            adc.getItems().clear();inputState.setText("Sem leitura das entradas.");
            if(snapshot!=null&&!connected&&device!=null)feedback.setText("ECU desligada · cópia local mantida para edição ou ficheiro.");
            else if(snapshot!=null&&connected&&!Objects.equals(loadedDevice,identity))feedback.setText("Cópia local de outra ECU · lê a configuração desta centralina antes de aplicar.");
        }
        device=connected?identity:null;supported=connected&&EnginePreviewClient.supports(identity);this.busy=busy;refresh();
    }
    void loaded(ConfigurationClient.Snapshot value) {
        snapshot=value;loadedDevice=device;base=value.configuration();baseline=base;display(base);
        feedback.setText(value.validation().dirty()?"Existem valores pendentes na ECU. Não serão substituídos automaticamente."
            :"Configuração lida · revisão "+value.validation().generation()+" · "+(value.validation().persisted()?"persistida na flash":"ainda não persistida"));
        validation.setText(value.validation().explanation());refresh();
    }
    private void display(IoConfiguration config) {
        loading=true;
        try {
            trigger.setSelected(config.triggerEnabled());map.setValue(config.mapChannel());
            lambda.setValue(config.lambdaChannel());coolant.setValue(config.coolantChannel());
            fields.forEach((field,editor)->editor.setText(config.value(field)));
        }finally{loading=false;}
    }
    IoConfiguration candidate() {

        EnumMap<IoConfiguration.Field,String> values=new EnumMap<>(IoConfiguration.Field.class);
        fields.forEach((field,editor)->values.put(field,editor.getText()));
        return base.edited(values,trigger.isSelected(),map.getValue(),
            lambda.getValue(),coolant.getValue());
    }
    private void refresh() {
        if(loading)return;
        boolean editable=!busy;
        boolean sameDevice=supported&&snapshot!=null&&Objects.equals(device,loadedDevice);
        boolean valid=false,changed=false;
        try{IoConfiguration value=candidate();valid=true;changed=snapshot!=null&&!Arrays.equals(value.bytes(),snapshot.configuration().bytes());}
        catch(IllegalArgumentException error){if(editable)feedback.setText(error.getMessage());}
        read.setDisable(!supported||busy);readInputs.setDisable(!supported||busy);pointButtons.forEach(b->b.setDisable(!supported||busy));
        apply.setDisable(!editable||!sameDevice||!valid||!changed||snapshot.validation().dirty());
        discard.setDisable(!editable||!sameDevice||!snapshot.validation().dirty());
        newLocal.setDisable(!editable);reset.setDisable(!editable);saveFile.setDisable(!editable||!valid);openFile.setDisable(!editable);
        trigger.setDisable(!editable);for(ComboBox<Integer> choice:List.of(map,lambda,coolant))choice.setDisable(!editable);
        fields.values().forEach(field->field.setDisable(!editable));
    }
    private boolean confirm(String title,String text) {
        Alert alert=ConsoleBranding.alert(Alert.AlertType.CONFIRMATION,text);alert.initOwner(owner);alert.setHeaderText(title);
        return alert.showAndWait().orElse(ButtonType.CANCEL)==ButtonType.OK;
    }
    private void apply() {
        try {
            IoConfiguration updated=candidate();ConfigurationClient.Snapshot expected=snapshot;
            if(confirm("Guardar configuração na ECU?","Os sensores e a captura passam a usar estes valores. Os mapas permanecem na ECU; nenhuma saída de motor é ativada."))
                access.run((s,id)->ConfigurationClient.apply(s,id,expected,updated,Duration.ofSeconds(20)),value->{loaded(value);onSaved.accept(value.validation());feedback.setText("Configuração guardada e confirmada por leitura da ECU.");},this::failed);
        }catch(IllegalArgumentException error){failed(error);}
    }
    void showInputs(IoClient.Inputs value) {
        boolean valid=(value.status()&1)!=0;int[] counts=value.raw();adc.getItems().clear();
        for(int i=0;i<16;i++)adc.getItems().add(new AdcRow(i<11?"Entrada analógica "+(i+1):i==11?"Monitorização da alimentação":"Entrada de temperatura "+(i-11),
            valid?Integer.toString(counts[i]):"—",valid?String.format(Locale.ROOT,"%.3f",counts[i]*value.referenceMv()/4095000.0):"—"));
        inputState.setText("ADC "+(valid?"válido":"indisponível")+" · cambota "+((value.status()&8)!=0?"sincronizada":"sem sincronismo")
            +" · erros de captura: "+value.errors()+" · revisão "+value.generation());
    }
    private boolean file(boolean opening) {
        FileChooser chooser=new FileChooser();chooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("Configuração Tunuino","*.tncfg"));
        ConsoleFiles.in(chooser, io.tunuino.maintenance.AppPaths.configurations());
        chooser.setTitle(opening?"Abrir configuração":"Guardar nova configuração");if(!opening)chooser.setInitialFileName("tunuino-proteus-io.tncfg");
        File selected=opening?chooser.showOpenDialog(owner):chooser.showSaveDialog(owner);if(selected==null)return false;
        try {
            if(opening){base=IoConfiguration.read(selected.toPath());display(base);feedback.setText("Ficheiro carregado para edição local; guardar na ECU continua a exigir confirmação.");}
            else{candidate().writeNew(selected.toPath());feedback.setText("Configuração guardada em "+selected.getName());}
            refresh();return true;
        }catch(Exception error){failed(error);return false;}
    }
    private boolean hasLocalChanges() {
        try{return !Arrays.equals(candidate().bytes(),baseline.bytes());}
        catch(IllegalArgumentException error){return true;}
    }
    public boolean canClose() {
        if(!hasLocalChanges())return true;
        ButtonType save=new ButtonType("Guardar ficheiro"),discardLocal=new ButtonType("Descartar");
        Alert alert=ConsoleBranding.alert(Alert.AlertType.CONFIRMATION,"Guardar uma cópia da configuração editada antes de fechar?");
        alert.initOwner(owner);alert.setHeaderText("Configuração com alterações locais");
        alert.getButtonTypes().setAll(save,discardLocal,ButtonType.CANCEL);
        ButtonType answer=alert.showAndWait().orElse(ButtonType.CANCEL);
        return answer==discardLocal||(answer==save&&file(false));
    }
    private void failed(Throwable error){feedback.setText("Configuração: "+error.getMessage());refresh();}
}
