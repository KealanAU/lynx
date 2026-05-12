// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

package com.lynx.xelement.input

import android.text.Spannable
import android.view.KeyEvent
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.InputConnection
import android.view.inputmethod.InputConnectionWrapper
import android.widget.EditText

class LynxInputConnectionWrapper(target: InputConnection?, mutable: Boolean) :
    InputConnectionWrapper(target, mutable) {

    private var mBackspaceListener: BackspaceListener? = null
    private var mEditTextRelated: EditText? = null
    private var mKeyEventListener: KeyEventListener? = null

    interface BackspaceListener {
        fun onBackspace(): Boolean
    }

    interface KeyEventListener {
        fun onKeyEvent(keyCode: Int, action: Int, metaState: Int)
    }

    fun setKeyEventListener(listener: KeyEventListener?) {
        mKeyEventListener = listener
    }

    fun removeKeyEventListener() {
        mKeyEventListener = null
    }

    fun bindEditText(editText: EditText) {
        mEditTextRelated = editText
    }

    // Google English input method will not send sendKeyEvent.
    // It will be handled directly here
    override fun deleteSurroundingText(beforeLength: Int, afterLength: Int): Boolean {
        if (beforeLength == 1 && afterLength == 0) {
            if (mBackspaceListener != null && mBackspaceListener!!.onBackspace()) {
                return true
            }
            mKeyEventListener?.onKeyEvent(
                KeyEvent.KEYCODE_DEL, KeyEvent.ACTION_DOWN, 0)
            mKeyEventListener?.onKeyEvent(
                KeyEvent.KEYCODE_DEL, KeyEvent.ACTION_UP, 0)
        }
        return super.deleteSurroundingText(beforeLength, afterLength)
    }

    override fun commitText(text: CharSequence?, newCursorPosition: Int): Boolean {
        return super.commitText(text, newCursorPosition)
    }

    override fun setComposingText(text: CharSequence?, newCursorPosition: Int): Boolean {
        return super.setComposingText(text, newCursorPosition)
    }

    override fun setComposingRegion(start: Int, end: Int): Boolean {
        return super.setComposingRegion(start, end)
    }

    fun setBackspaceListener(backspaceListener: BackspaceListener?) {
        mBackspaceListener = backspaceListener
    }

    fun removeBackspaceListener() {
        mBackspaceListener = null
    }

    // Intercept the del event
    override fun sendKeyEvent(event: KeyEvent): Boolean {
        if (event.keyCode == KeyEvent.KEYCODE_DEL && event.action == KeyEvent.ACTION_DOWN) {
            if (mBackspaceListener != null && mBackspaceListener!!.onBackspace()) {
                return true
            }
        }
        mKeyEventListener?.onKeyEvent(event.keyCode, event.action, event.metaState)
        return super.sendKeyEvent(event)
    }

    /**
     * Check whether there is ComposingText in `text`
     *
     * We must clarify a rule. All our string operations only need to consider whether the newer
     * text is ComposingText. Because the ComposingText state of the old (modified) text should be
     * cleared directly. Only need to determine whether the result should be ComposingText based
     * on whether the newer is ComposingText.
     *
     * More information about ComposingText:
     * ComposingText is NoCopySpan. A valid ComposingText must have BaseInputConnection.COMPOSING.
     * Therefore you must restore the string after you manipulate it!
     * Otherwise the text in the input box is not considered ComposingText. This will cause all
     * ComposingText to be re-entered.
     *
     * Refer to BaseInputConnection.replaceText.
     */
    fun hasComposingText(text: Spannable): Boolean {
        val a = BaseInputConnection.getComposingSpanStart(text)
        val b = BaseInputConnection.getComposingSpanEnd(text)
        return a != -1 && b != -1
    }
}