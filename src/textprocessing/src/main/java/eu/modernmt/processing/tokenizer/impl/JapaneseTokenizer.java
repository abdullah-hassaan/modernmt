package eu.modernmt.processing.tokenizer.impl;

import eu.modernmt.lang.Language;
import eu.modernmt.lang.UnsupportedLanguageException;
import eu.modernmt.processing.tokenizer.BaseTokenizer;
import eu.modernmt.processing.tokenizer.jflex.annotators.CommonTermsTokenAnnotator;
import eu.modernmt.processing.tokenizer.kuromoji.KuromojiTokenAnnotator;

import java.io.Reader;

public class JapaneseTokenizer extends BaseTokenizer {

    public JapaneseTokenizer(Language sourceLanguage, Language targetLanguage) throws UnsupportedLanguageException {
        super(sourceLanguage, targetLanguage);

        super.annotators.add(new KuromojiTokenAnnotator());
        super.annotators.add(new CommonTermsTokenAnnotator((Reader) null));
    }
}
