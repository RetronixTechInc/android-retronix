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

package com.koushikdutta.widgets;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.View;

public interface FragmentInterfaceWrapper {
    public Activity getActivity();
    public Context getContext();
    public FragmentInterface getInternal();
    public void setHasOptionsMenu(boolean options);
    
    void setArguments(Bundle bundle);
    Bundle getArguments();
    View getView();
    
    int getId();
}
