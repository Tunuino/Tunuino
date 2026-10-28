/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.maintenance.AppPaths;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.*;
import java.util.regex.Pattern;
import javafx.beans.property.StringProperty;
import javafx.collections.ListChangeListener;
import javafx.scene.Node;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.*;

/** Translates display properties only; values, protocols, maps and routing keys stay intact. */
final class ConsoleLanguage {
    enum Language { PORTUGUESE("Português"),ENGLISH("English");
        private final String label;Language(String label){this.label=label;}@Override public String toString(){return label;}
    }
    private static final Map<String,String> english=new LinkedHashMap<>();
    private static final Map<StringProperty,String> sources=new WeakHashMap<>();
    private static final Set<StringProperty> changing=Collections.newSetFromMap(new WeakHashMap<>());
    private static final Set<Object> observed=Collections.newSetFromMap(new WeakHashMap<>());
    private static final Set<ComboBox<Language>> selectors=Collections.newSetFromMap(new WeakHashMap<>());
    private static final java.util.List<Runnable> redraws=new ArrayList<>();
    private static final Pattern phrases;
    private static Language language=load();
    private static boolean selecting;
    static {
        try(var stream=ConsoleLanguage.class.getResourceAsStream("english.tsv")) {
            if(stream==null)throw new IllegalStateException("Missing English interface catalogue");
            for(String line:new String(stream.readAllBytes(),StandardCharsets.UTF_8).split("\\R")) {
                if(line.isBlank()||line.startsWith("#"))continue;
                String[] pair=line.split("\t",2);if(pair.length!=2)throw new IllegalStateException("Invalid English catalogue entry");
                english.put(pair[0].replace("\\n","\n"),pair[1].replace("\\n","\n"));
            }
        }catch(IOException error){throw new IllegalStateException("Cannot load English interface catalogue",error);}
        for(var entry:new ArrayList<>(english.entrySet())) {
            english.putIfAbsent(entry.getKey().toUpperCase(Locale.ROOT),entry.getValue().toUpperCase(Locale.ROOT));
            english.putIfAbsent(entry.getKey().toLowerCase(Locale.ROOT),entry.getValue().toLowerCase(Locale.ROOT));
        }
        var keys=new ArrayList<>(english.keySet());keys.sort(Comparator.comparingInt(String::length).reversed());
        phrases=Pattern.compile("(?<![\\p{L}])(?:"+String.join("|",keys.stream().map(Pattern::quote).toList())+")(?![\\p{L}])");
    }
    private ConsoleLanguage() {}
    static Language current(){return language;}
    static String text(String source) {
        if(source==null||language==Language.PORTUGUESE)return source;
        String exact=english.get(source);if(exact!=null)return exact;
        return phrases.matcher(source).replaceAll(match->java.util.regex.Matcher.quoteReplacement(english.get(match.group())));
    }
    static void use(Language selected,boolean persist) {
        language=Objects.requireNonNull(selected);
        selecting=true;try{for(var selector:List.copyOf(selectors))selector.setValue(selected);}finally{selecting=false;}
        for(var entry:new ArrayList<>(sources.entrySet()))render(entry.getKey(),entry.getValue());
        for(var redraw:List.copyOf(redraws))redraw.run();
        if(persist)try {
            Files.createDirectories(AppPaths.data());
            Files.writeString(AppPaths.data().resolve("language.txt"),selected.name(),StandardCharsets.UTF_8);
        }catch(IOException error){throw new IllegalStateException("Não foi possível guardar a língua escolhida",error);}
    }
    private static Language load() {
        try{return Language.valueOf(Files.readString(AppPaths.data().resolve("language.txt"),StandardCharsets.UTF_8).trim());}
        catch(IOException|IllegalArgumentException error){return Language.PORTUGUESE;}
    }
    static void watch(StringProperty property) {
        if(sources.containsKey(property))return;
        sources.put(property,property.get());
        property.addListener((p,before,value)->{
            if(changing.contains(property))return;
            sources.put(property,value);render(property,value);
        });
        render(property,property.get());
    }
    private static void render(StringProperty property,String source) {
        if(property.isBound())return;
        changing.add(property);try{property.set(text(source));}finally{changing.remove(property);}
    }
    static void redrawWithLanguage(Runnable redraw){redraws.add(redraw);}
    static ComboBox<Language> selector() {
        var selector=new ComboBox<Language>();selector.getItems().addAll(Language.values());selector.setValue(language);
        selector.setId("console-language");selector.setPrefWidth(136);selector.setMinWidth(136);selector.setAccessibleText("Português / English");
        selectors.add(selector);
        selector.setOnAction(e->{if(selecting)return;try{use(selector.getValue(),true);}catch(IllegalStateException error){
            var alert=ConsoleBranding.alert(Alert.AlertType.WARNING,error.getMessage());alert.show();
        }});return selector;
    }
    static void install(Scene scene){observe(scene.getRoot());}
    static void observe(Node node) {
        if(!observed.add(node))return;
        if(node instanceof Labeled label)watch(label.textProperty());
        if(node instanceof Control control){
            if(control.getTooltip()!=null)watch(control.getTooltip().textProperty());
            control.tooltipProperty().addListener((p,a,b)->{if(b!=null)watch(b.textProperty());});
        }
        if(node instanceof TabPane tabs) {
            tabs.getTabs().forEach(ConsoleLanguage::tab);
            tabs.getTabs().addListener((ListChangeListener<Tab>)c->{while(c.next())for(var tab:c.getAddedSubList())tab(tab);});
        }
        if(node instanceof TableView<?> table) {
            table.getColumns().forEach(ConsoleLanguage::column);
            table.getColumns().addListener((ListChangeListener<TableColumn<?,?>>)c->{while(c.next())c.getAddedSubList().forEach(ConsoleLanguage::column);});
        }
        if(node instanceof ComboBox<?> combo)combo(combo);
        if(node instanceof SplitPane split){split.getItems().forEach(ConsoleLanguage::observe);split.getItems().addListener((ListChangeListener<Node>)c->{while(c.next())c.getAddedSubList().forEach(ConsoleLanguage::observe);});}
        if(node instanceof ToolBar bar){bar.getItems().forEach(ConsoleLanguage::observe);bar.getItems().addListener((ListChangeListener<Node>)c->{while(c.next())c.getAddedSubList().forEach(ConsoleLanguage::observe);});}
        if(node instanceof Labeled label&&label.getGraphic()!=null)observe(label.getGraphic());
        if(node instanceof DialogPane dialog){for(var type:dialog.getButtonTypes())observe(dialog.lookupButton(type));if(dialog.getContent()!=null)observe(dialog.getContent());}
        if(node instanceof MenuBar bar)bar.getMenus().forEach(ConsoleLanguage::menu);
        if(node instanceof ScrollPane scroll){if(scroll.getContent()!=null)observe(scroll.getContent());scroll.contentProperty().addListener((p,a,b)->{if(b!=null)observe(b);});}
        if(node instanceof TitledPane pane){if(pane.getContent()!=null)observe(pane.getContent());pane.contentProperty().addListener((p,a,b)->{if(b!=null)observe(b);});}
        // Traverse application content explicitly, never skin-owned copies of labels.
        if(node instanceof Parent parent&&!(node instanceof Control)) {
            parent.getChildrenUnmodifiable().forEach(ConsoleLanguage::observe);
            parent.getChildrenUnmodifiable().addListener((ListChangeListener<Node>)c->{while(c.next())c.getAddedSubList().forEach(ConsoleLanguage::observe);});
        }
    }
    private static void tab(Tab tab){watch(tab.textProperty());if(tab.getContent()!=null)observe(tab.getContent());tab.contentProperty().addListener((p,a,b)->{if(b!=null)observe(b);});}
    @SuppressWarnings({"rawtypes","unchecked"})
    private static void column(TableColumn<?,?> column){
        watch(column.textProperty());column.getColumns().forEach(ConsoleLanguage::column);
        TableColumn raw=column;javafx.util.Callback<TableColumn,TableCell> factory=raw.getCellFactory();
        raw.setCellFactory((javafx.util.Callback<TableColumn,TableCell>)c->{TableCell cell=factory.call(c);watch(cell.textProperty());return cell;});
    }
    private static <T> void combo(ComboBox<T> combo) {
        if(combo.isEditable())return;
        java.util.function.Supplier<ListCell<T>> create=()->{
            var cell=new ListCell<T>(){@Override protected void updateItem(T value,boolean empty){super.updateItem(value,empty);
                setText(empty||value==null?null:combo.getConverter().toString(value));}};
            watch(cell.textProperty());return cell;
        };
        combo.setCellFactory(view->create.get());combo.setButtonCell(create.get());
    }
    private static void menu(MenuItem menu){watch(menu.textProperty());if(menu instanceof Menu parent)parent.getItems().forEach(ConsoleLanguage::menu);}
}
