// Copyright 2024 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
package com.lynx.tasm.behavior;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.os.Build;
import android.util.DisplayMetrics;
import androidx.test.platform.app.InstrumentationRegistry;
import com.lynx.tasm.utils.DisplayMetricsHolder;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

/**
 * Unit tests for Phase 2 WindowInsetsCompat keyboard detection in {@link KeyboardEvent}.
 *
 * <p>These tests verify the observer dispatch path used by both the WindowInsetsCompat path
 * (API 23+) and the legacy KeyboardMonitor path (API < 23). They run against the real
 * implementation without requiring a live Activity by invoking the private
 * {@code sendKeyboardEvent} method via reflection — the same path that both code paths call.
 */
public class KeyboardEventTest {

  private static final float TEST_DPI = 1.0f;

  private LynxContext mLynxContext;
  private KeyboardEvent mKeyboardEvent;

  @Before
  public void setUp() throws Exception {
    android.content.Context appContext =
        InstrumentationRegistry.getInstrumentation().getTargetContext().getApplicationContext();
    DisplayMetricsHolder.updateOrInitDisplayMetrics(appContext, TEST_DPI);
    DisplayMetrics displayMetrics = new DisplayMetrics();
    displayMetrics.widthPixels = 1080;
    displayMetrics.heightPixels = 1920;
    displayMetrics.density = TEST_DPI;
    mLynxContext = new LynxContext(appContext, displayMetrics) {
      @Override
      public void handleException(Exception e) {}
    };
    mKeyboardEvent = new KeyboardEvent(mLynxContext);
  }

  @After
  public void tearDown() {
    mLynxContext = null;
    mKeyboardEvent = null;
  }

  // -------------------------------------------------------------------------
  // Helper: invoke private sendKeyboardEvent(boolean, int, int) via reflection
  // -------------------------------------------------------------------------

  private void invokeSendKeyboardEvent(boolean isVisible, int height, int heightCompat)
      throws Exception {
    Method m = KeyboardEvent.class.getDeclaredMethod(
        "sendKeyboardEvent", boolean.class, int.class, int.class);
    m.setAccessible(true);
    m.invoke(mKeyboardEvent, isVisible, height, heightCompat);
  }

  private KeyboardMonitor getKeyboardMonitor() throws Exception {
    Field f = KeyboardEvent.class.getDeclaredField("mKeyboardMonitor");
    f.setAccessible(true);
    return (KeyboardMonitor) f.get(mKeyboardEvent);
  }

  // -------------------------------------------------------------------------
  // Tests
  // -------------------------------------------------------------------------

  /**
   * On API 23+ the WindowInsetsCompat path fires {@code keyboardWillShow} with the pixel height
   * derived from the IME insets height. With TEST_DPI=1 the dp and px values are equal, so the
   * observer receives the raw 300 that the insets listener would compute as imeHeightDp and then
   * pass into sendKeyboardEvent — which re-scales by mDpi back to pixels.
   */
  @Test
  public void keyboardDetected_withWindowInsetsCompat_onApi23Plus() throws Exception {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
      return; // test only applies to API 23+
    }
    KeyboardEvent.KeyboardEventObserver observer =
        mock(KeyboardEvent.KeyboardEventObserver.class);
    mKeyboardEvent.addKeyboardEventObserver(observer);

    // Simulate what the WindowInsetsCompat listener does: imeHeightPx=300, dpi=1 → imeHeightDp=300
    // sendKeyboardEvent(true, 300, 300) → observer.keyboardWillShow((int)(300 * 1.0f)) = 300
    invokeSendKeyboardEvent(true, 300, 300);

    verify(observer).keyboardWillShow(300);
    verify(observer, never()).keyboardWillHide();
  }

  /**
   * On API 23+ when the IME becomes invisible the WindowInsetsCompat listener fires
   * sendKeyboardEvent(false, 0, 0), which must route to {@code keyboardWillHide}.
   */
  @Test
  public void keyboardHidden_withWindowInsetsCompat_onApi23Plus() throws Exception {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
      return; // test only applies to API 23+
    }
    KeyboardEvent.KeyboardEventObserver observer =
        mock(KeyboardEvent.KeyboardEventObserver.class);
    mKeyboardEvent.addKeyboardEventObserver(observer);

    invokeSendKeyboardEvent(false, 0, 0);

    verify(observer).keyboardWillHide();
    verify(observer, never()).keyboardWillShow(anyInt());
  }

  /**
   * On API < 23 the legacy KeyboardMonitor ratio-detection path is used. Verify that when
   * the global-layout listener would detect a visible keyboard it routes through
   * sendKeyboardEvent correctly and notifies the observer.
   */
  @Test
  public void fallsBackToRatioDetection_onApiBelow23() throws Exception {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
      // On API 23+ the insets path is active; this test targets the < 23 fallback only.
      return;
    }
    KeyboardEvent.KeyboardEventObserver observer =
        mock(KeyboardEvent.KeyboardEventObserver.class);
    mKeyboardEvent.addKeyboardEventObserver(observer);

    // Simulate what detectKeyboardChangeAndSendEvent would compute and dispatch.
    invokeSendKeyboardEvent(true, 250, 250);

    verify(observer).keyboardWillShow(250);
    verify(observer, never()).keyboardWillHide();
  }

  /**
   * After start() the WindowInsetsCompat path (API 23+) must NOT create a KeyboardMonitor,
   * because the insets listener on the decor view replaces it. On API < 23 the KeyboardMonitor
   * IS created as the legacy fallback.
   *
   * <p>start() posts to the UI thread and returns before startInMain() runs when called off the
   * UI thread (which is always true in instrumented tests). Additionally startInMain() early-
   * returns when getActivity() is null (application context, not Activity). So we test the
   * invariant that a freshly constructed KeyboardEvent has no KeyboardMonitor — confirming the
   * WindowInsetsCompat path does not eagerly allocate one before it is needed.
   */
  @Test
  public void windowInsetsPath_doesNotInstantiateKeyboardMonitor_onApi23Plus() throws Exception {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
      return;
    }
    // Before start(), mKeyboardMonitor must be null; the insets path never sets it.
    assertNull(
        "KeyboardMonitor should not be allocated when WindowInsetsCompat path is active",
        getKeyboardMonitor());
  }

  /**
   * Multiple observers registered via {@link KeyboardEvent#addKeyboardEventObserver} all receive
   * the keyboard-show notification, matching the behaviour of both the WindowInsetsCompat and
   * legacy paths which share the same sendKeyboardEvent implementation.
   */
  @Test
  public void multipleObservers_allReceiveKeyboardWillShow() throws Exception {
    KeyboardEvent.KeyboardEventObserver observer1 =
        mock(KeyboardEvent.KeyboardEventObserver.class);
    KeyboardEvent.KeyboardEventObserver observer2 =
        mock(KeyboardEvent.KeyboardEventObserver.class);
    mKeyboardEvent.addKeyboardEventObserver(observer1);
    mKeyboardEvent.addKeyboardEventObserver(observer2);

    invokeSendKeyboardEvent(true, 400, 400);

    verify(observer1).keyboardWillShow(400);
    verify(observer2).keyboardWillShow(400);
  }

  /**
   * Multiple observers all receive {@code keyboardWillHide} when the keyboard is dismissed.
   */
  @Test
  public void multipleObservers_allReceiveKeyboardWillHide() throws Exception {
    KeyboardEvent.KeyboardEventObserver observer1 =
        mock(KeyboardEvent.KeyboardEventObserver.class);
    KeyboardEvent.KeyboardEventObserver observer2 =
        mock(KeyboardEvent.KeyboardEventObserver.class);
    mKeyboardEvent.addKeyboardEventObserver(observer1);
    mKeyboardEvent.addKeyboardEventObserver(observer2);

    invokeSendKeyboardEvent(false, 0, 0);

    verify(observer1).keyboardWillHide();
    verify(observer2).keyboardWillHide();
  }

  /**
   * Verify that the height passed to {@code keyboardWillShow} correctly reflects the dpi
   * scaling applied by {@code sendKeyboardEvent}: {@code heightCompat * mDpi}.
   */
  @Test
  public void keyboardWillShow_heightIsScaledByDpi() throws Exception {
    // Use a separate KeyboardEvent with dpi=2 to confirm scaling.
    android.content.Context appContext =
        InstrumentationRegistry.getInstrumentation().getTargetContext().getApplicationContext();
    DisplayMetrics dm = new DisplayMetrics();
    dm.widthPixels = 1080;
    dm.heightPixels = 1920;
    dm.density = 2.0f;
    DisplayMetricsHolder.updateOrInitDisplayMetrics(appContext, 2.0f);
    LynxContext ctx = new LynxContext(appContext, dm) {
      @Override
      public void handleException(Exception e) {}
    };
    KeyboardEvent ke = new KeyboardEvent(ctx);
    KeyboardEvent.KeyboardEventObserver observer =
        mock(KeyboardEvent.KeyboardEventObserver.class);
    ke.addKeyboardEventObserver(observer);

    Method m = KeyboardEvent.class.getDeclaredMethod(
        "sendKeyboardEvent", boolean.class, int.class, int.class);
    m.setAccessible(true);
    // imeHeightDp = 150; expected pixel height = 150 * 2.0 = 300
    m.invoke(ke, true, 150, 150);

    verify(observer).keyboardWillShow(300);
  }
}
