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
    private final ComboBox<String> map=channels(false),lambda=channels(false),coolant=channels(true);
    private final Button read=new Button("Ler configuração"),apply=new Button("Guardar na ECU"),
        discard=new Button("Descartar pendentes da ECU"),reset=new Button("Repor edição"),
        saveFile=new Button("Guardar ficheiro"),openFile=new Button("Abrir ficheiro"),readInputs=new Button("Ler entradas ADC");
    private final Label feedback=new Label("Liga e identifica a ECU para ler a configuração."),
        validation=new Label("Sem validação recebida do firmware."),inputState=new Label("Sem leitura das entradas.");
    private final TableView<AdcRow> adc=new TableView<>();
    private ConfigurationClient.Snapshot snapshot;
    private DeviceIdentity device;
    private DeviceIdentity loadedDevice;
    private boolean supported,busy,loading;

    public ConfigurationPane(Stage owner,Access access) {
        this.owner=owner;this.access=access;setSpacing(16);setPadding(new Insets(0,24,24,24));
        read.setId("config-read");apply.setId("config-apply");discard.setId("config-discard");
        feedback.setId("config-feedback");validation.setId("config-validation");readInputs.setId("config-read-adc");
        trigger.setId("config-trigger");map.setId("config-map-channel");lambda.setId("config-lambda-channel");coolant.setId("config-coolant-channel");
        apply.getStyleClass().add("primary");read.getStyleClass().add("primary");
        for(Label label:List.of(feedback,validation,inputState))label.setWrapText(true);
        getChildren().add(ConsoleShell.card("CONFIGURAÇÃO DA CENTRALINA",
            "Lê os valores antes de editar. Guardar valida, persiste e confirma a leitura; os mapas são preservados.",
            ConsoleShell.actions(read,apply,reset,openFile,saveFile),feedback,validation,discard));
        TilePane forms=new TilePane(16,16);forms.setId("config-form-grid");forms.setPrefColumns(2);
        forms.setPrefTileWidth(420);
        forms.widthProperty().addListener((p,old,width)->{
            double available=width.doubleValue();
            if(available>0)forms.setPrefTileWidth(Math.floor(available>=800?(available-16)/2:available));
        });
        for(String group:List.of("Aquisição","MAP","Lambda","Temperatura")) {
            GridPane grid=new GridPane();grid.setHgap(16);grid.setVgap(10);int row=0;
            if(group.equals("Aquisição"))grid.add(trigger,0,row++,2,1);
            else {ComboBox<String> selector=group.equals("MAP")?map:group.equals("Lambda")?lambda:coolant;
                grid.add(new Label("Canal de entrada"),0,row);grid.add(selector,1,row++);}
            for(IoConfiguration.Field field:IoConfiguration.Field.values())if(field.group.equals(group)) {
                TextField editor=new TextField();editor.setPrefColumnCount(10);editor.setMaxWidth(160);
                editor.setId("config-field-"+field.name());fields.put(field,editor);
                grid.add(new Label(field.label),0,row);grid.add(editor,1,row++);
                editor.textProperty().addListener((p,old,value)->{if(!loading)refresh();});
            }
            String subtitle=group.equals("Aquisição")?"Configuração de aquisição; não ativa injetores, bobinas ou outras saídas."
                :group.equals("Temperatura")?"Três pontos ADC/temperatura. Usa a curva medida do teu sensor."
                :"Dois pontos ADC/valor físico. Usa ponto decimal e a calibração real do sensor.";
            VBox card=ConsoleShell.card(group.toUpperCase(Locale.ROOT),subtitle,grid);
            card.prefWidthProperty().bind(forms.prefTileWidthProperty());forms.getChildren().add(card);
        }
        getChildren().add(forms);
        TableColumn<AdcRow,String> name=new TableColumn<>("Entrada"),raw=new TableColumn<>("ADC"),volts=new TableColumn<>("Pino MCU (V)");
        name.setCellValueFactory(v->new SimpleStringProperty(v.getValue().channel()));
        raw.setCellValueFactory(v->new SimpleStringProperty(v.getValue().raw()));
        volts.setCellValueFactory(v->new SimpleStringProperty(v.getValue().volts()));
        adc.getColumns().setAll(List.of(name,raw,volts));adc.setColumnResizePolicy(TableView.CONSTRAINED_RESIZE_POLICY_FLEX_LAST_COLUMN);
        adc.setPrefHeight(360);adc.setId("config-adc-table");
        getChildren().add(ConsoleShell.card("ENTRADAS E ESTADO", "A tensão mostrada é no pino MCU; os divisores externos não são inferidos.",readInputs,inputState,adc));
        trigger.selectedProperty().addListener((p,a,b)->refresh());
        for(ComboBox<String> selector:List.of(map,lambda,coolant))selector.setOnAction(e->{if(!loading)refresh();});
        read.setOnAction(e->{
            if(hasLocalChanges()&&!confirm("Repor edição local?","A leitura substituirá a edição local pelos valores da ECU."))return;
            access.run((s,id)->ConfigurationClient.read(s,id,Duration.ofSeconds(5)),this::loaded,this::failed);
        });
        apply.setOnAction(e->apply());reset.setOnAction(e->{if(snapshot!=null){display(snapshot.configuration());feedback.setText("Edição local reposta; a ECU não foi alterada.");refresh();}});
        discard.setOnAction(e->{
            if(confirm("Descartar os valores pendentes na ECU?","As alterações pendentes de configuração e mapas serão substituídas pelos valores ativos."))
                access.run((s,id)->ConfigurationClient.discard(s,id,Duration.ofSeconds(5)),this::loaded,this::failed);
        });
        readInputs.setOnAction(e->access.run((s,id)->IoClient.read(s,id,Duration.ofSeconds(2)),this::showInputs,this::failed));
        saveFile.setOnAction(e->file(false));openFile.setOnAction(e->file(true));
        refresh();
    }
    private static ComboBox<String> channels(boolean temperature) {
        ComboBox<String> choice=new ComboBox<>();choice.getItems().add("Desativado");
        for(int i=1;i<=(temperature?4:11);i++)choice.getItems().add((temperature?"AT":"AV")+i);
        choice.getSelectionModel().select(0);return choice;
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
        snapshot=value;loadedDevice=device;display(value.configuration());
        feedback.setText(value.validation().dirty()?"Existem valores pendentes na ECU. Não serão substituídos automaticamente."
            :"Configuração lida · revisão "+value.validation().generation()+" · "+(value.validation().persisted()?"persistida na flash":"ainda não persistida"));
        validation.setText(value.validation().explanation());refresh();
    }
    private void display(IoConfiguration config) {
        loading=true;
        try {
            trigger.setSelected(config.triggerEnabled());map.getSelectionModel().select(config.mapChannel());
            lambda.getSelectionModel().select(config.lambdaChannel());coolant.getSelectionModel().select(config.coolantChannel());
            fields.forEach((field,editor)->editor.setText(config.value(field)));
        }finally{loading=false;}
    }
    IoConfiguration candidate() {
        if(snapshot==null)throw new IllegalStateException("Lê primeiro a configuração da ECU");
        EnumMap<IoConfiguration.Field,String> values=new EnumMap<>(IoConfiguration.Field.class);
        fields.forEach((field,editor)->values.put(field,editor.getText()));
        return snapshot.configuration().edited(values,trigger.isSelected(),map.getSelectionModel().getSelectedIndex(),
            lambda.getSelectionModel().getSelectedIndex(),coolant.getSelectionModel().getSelectedIndex());
    }
    private void refresh() {
        if(loading)return;
        boolean editable=!busy&&snapshot!=null;
        boolean sameDevice=supported&&Objects.equals(device,loadedDevice);
        boolean valid=false,changed=false;
        if(snapshot!=null)try{IoConfiguration value=candidate();valid=true;changed=!Arrays.equals(value.bytes(),snapshot.configuration().bytes());}
        catch(IllegalArgumentException error){if(editable)feedback.setText(error.getMessage());}
        read.setDisable(!supported||busy);readInputs.setDisable(!supported||busy);
        apply.setDisable(!editable||!sameDevice||!valid||!changed||snapshot.validation().dirty());
        discard.setDisable(!editable||!sameDevice||!snapshot.validation().dirty());
        reset.setDisable(!editable);saveFile.setDisable(!editable||!valid);openFile.setDisable(!editable);
        trigger.setDisable(!editable);for(ComboBox<String> choice:List.of(map,lambda,coolant))choice.setDisable(!editable);
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
                access.run((s,id)->ConfigurationClient.apply(s,id,expected,updated,Duration.ofSeconds(20)),value->{loaded(value);feedback.setText("Configuração guardada e confirmada por leitura da ECU.");},this::failed);
        }catch(IllegalArgumentException error){failed(error);}
    }
    void showInputs(IoClient.Inputs value) {
        boolean valid=(value.status()&1)!=0;int[] counts=value.raw();adc.getItems().clear();
        for(int i=0;i<16;i++)adc.getItems().add(new AdcRow(i<11?"AV"+(i+1):i==11?"12V dividido":"AT"+(i-11),
            valid?Integer.toString(counts[i]):"—",valid?String.format(Locale.ROOT,"%.3f",counts[i]*value.referenceMv()/4095000.0):"—"));
        inputState.setText("ADC "+(valid?"válido":"indisponível")+" · cambota "+((value.status()&8)!=0?"sincronizada":"sem sincronismo")
            +" · erros de captura: "+value.errors()+" · revisão "+value.generation());
    }
    private boolean file(boolean opening) {
        FileChooser chooser=new FileChooser();chooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("Configuração Tunuino","*.tncfg"));
        chooser.setTitle(opening?"Abrir configuração":"Guardar nova configuração");if(!opening)chooser.setInitialFileName("tunuino-proteus-io.tncfg");
        File selected=opening?chooser.showOpenDialog(owner):chooser.showSaveDialog(owner);if(selected==null)return false;
        try {
            if(opening){display(IoConfiguration.read(selected.toPath()));feedback.setText("Ficheiro carregado para edição local; guardar na ECU continua a exigir confirmação.");}
            else{candidate().writeNew(selected.toPath());feedback.setText("Configuração guardada em "+selected.getName());}
            refresh();return true;
        }catch(Exception error){failed(error);return false;}
    }
    private boolean hasLocalChanges() {
        if(snapshot==null)return false;
        try{return !Arrays.equals(candidate().bytes(),snapshot.configuration().bytes());}
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
