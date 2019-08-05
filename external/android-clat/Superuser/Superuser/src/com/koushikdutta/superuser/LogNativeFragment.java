/*
 * Copyright (C) 2013 Koushik Dutta (@koush)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.koushikdutta.superuser;

import android.annotation.SuppressLint;
import android.app.Fragment;
import android.content.Context;
import android.os.Bundle;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.koushikdutta.widgets.NativeFragment;

@SuppressLint("NewApi")
public class LogNativeFragment extends NativeFragment<LogFragmentInternal> {
    ContextThemeWrapper mWrapper;
    public Context getContext() {
        if (mWrapper != null)
            return mWrapper;
        mWrapper = new ContextThemeWrapper(super.getContext(), R.style.SuperuserDark);
        return mWrapper;
    }

    @Override
    public LogFragmentInternal createFragmentInterface() {
        return new LogFragmentInternal(this) {
            @Override
            public Context getContext() {
                return LogNativeFragment.this.getContext();
            }

            @Override
            void onDelete() {
                super.onDelete();
                LogNativeFragment.this.onDelete(getListContentId());
            }
        };
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return super.onCreateView((LayoutInflater)getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE), container, savedInstanceState);
    }

    void onDelete(int id) {
//        getFragmentManager().beginTransaction().remove(this).commit();
//        getFragmentManager().popBackStack("content", FragmentManager.POP_BACK_STACK_INCLUSIVE);
        Fragment f = getFragmentManager().findFragmentById(id);
        if (f != null && f instanceof PolicyNativeFragment) {
            PolicyNativeFragment p = (PolicyNativeFragment)f;
            ((PolicyFragmentInternal)p.getInternal()).load();
            ((PolicyFragmentInternal)p.getInternal()).showAllLogs();
        }
    }
}
