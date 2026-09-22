var i18n = {
    zh: window.i18n_zh || {},
    en: window.i18n_en || {}
};

var curLang = localStorage.getItem('app_lang') || (navigator.language.startsWith('zh') ? 'zh' : 'en');

function t(key, vars) {
    var val = (i18n[curLang] && i18n[curLang][key]) || (i18n['en'] && i18n['en'][key]) || key;
    if (vars) {
        for (var k in vars) {
            val = val.replace('{' + k + '}', vars[k]);
        }
    }
    return val;
}

function applyI18n() {
    document.querySelectorAll('[data-i18n]').forEach(el => {
        var k = el.getAttribute('data-i18n');
        if (i18n[curLang] && i18n[curLang][k]) el.innerText = i18n[curLang][k];
    });
    document.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
        var k = el.getAttribute('data-i18n-placeholder');
        if (i18n[curLang] && i18n[curLang][k]) el.placeholder = i18n[curLang][k];
    });
    document.getElementById('lang-switch').value = curLang;
    document.title = t('title');
}

function switchLang(lang) {
    curLang = lang;
    localStorage.setItem('app_lang', lang);
    applyI18n();
}
